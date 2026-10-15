#include "interferogram/config.hpp"
#include "interferogram/result_fingerprint.hpp"
#include "interferogram/slc_reader.hpp"
#include "interferogram/interferogram.hpp"
#include "interferogram/models/fine_grained.hpp"
#include "interferogram/models/coarse_grained.hpp"
#include "interferogram/models/smt.hpp"
#include "interferogram/models/cmp.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <memory>
#include <cmath>
#include <gdal_priv.h>
#include <filesystem>
#include <vector>

namespace {
// Guarda el vector calculado por el modelo, sin volver a calcular el producto.
void exportResult(const std::string& path,
                  const std::vector<interferogram::Complex>& result,
                  const interferogram::Config& config) {
    if (std::filesystem::exists(path)) {
        throw std::runtime_error("La salida ya existe: " + path);
    }

    const auto directory = std::filesystem::path(path).parent_path();
    if (!directory.empty()) {
        std::filesystem::create_directories(directory);
    }

    GDALAllRegister();
    auto* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
    if (!driver) {
        throw std::runtime_error("Driver GTiff no disponible");
    }

    char tiled[] = "TILED=YES";
    char bigTiff[] = "BIGTIFF=IF_SAFER";
    char* options[] = {tiled, bigTiff, nullptr};
    std::unique_ptr<GDALDataset, decltype(&GDALClose)> dataset(
        driver->Create(path.c_str(), config.width, config.height, 1, GDT_CFloat64, options),
        GDALClose);
    if (!dataset) {
        throw std::runtime_error("No se pudo crear " + path);
    }

    dataset->SetMetadataItem("MODEL", config.mode.c_str());
    dataset->SetMetadataItem("SOURCE_A", config.a.c_str());
    dataset->SetMetadataItem("SOURCE_B", config.b.c_str());
    dataset->SetMetadataItem("OFFSET_X", std::to_string(config.x).c_str());
    dataset->SetMetadataItem("OFFSET_Y", std::to_string(config.y).c_str());
    dataset->SetMetadataItem("PROCESSING", "C++ kernel A * conjugate(B); radar coordinates");

    // GDAL recibe cada muestra como un par de doubles: real, imaginaria.
    auto* band = dataset->GetRasterBand(1);
    std::vector<double> row(static_cast<std::size_t>(config.width) * 2);
    for (int y = 0; y < config.height; ++y) {
        const std::size_t offset = static_cast<std::size_t>(y) * config.width;
        for (int x = 0; x < config.width; ++x) {
            const auto& sample = result[offset + x];
            row[2 * x] = sample.real;
            row[2 * x + 1] = sample.imag;
        }

        const auto status = band->RasterIO(
            GF_Write, 0, y, config.width, 1, row.data(),
            config.width, 1, GDT_CFloat64, 0, 0, nullptr);
        if (status != CE_None) {
            throw std::runtime_error("Error escribiendo " + path);
        }
    }
    dataset->FlushCache();
}
} // namespace

int main(int argc, char* argv[]) {
    using namespace interferogram;
    try {
        const Config config = parseConfig(argc, argv);
        if (config.help) {
            printHelp(std::cout);
            return 0;
        }
        // Usar las mismas coordenadas en A y B; esto no realiza corregistro.
        const auto a = readSlcWindow(config.a, config.x, config.y, config.width, config.height, config.allowNoData);
        const auto b = readSlcWindow(config.b, config.x, config.y, config.width, config.height, config.allowNoData);
        // Preparar y validar la salida antes de iniciar el cronómetro.
        std::vector<Complex> result(a.size());
        validateInterferogramInputs(a, b, result);

        // Preparar la partición fuera de la medición del kernel.
        const auto tiles = config.mode == "tiled"
            ? makeTiles(config.width, config.height, config.tileWidth, config.tileHeight)
            : std::vector<Tile>{};

        std::unique_ptr<FineGrained> fineGrained;
        if (config.mode == "fine-grained") {
            fineGrained = std::make_unique<FineGrained>(config.width, config.height,
                config.tileWidth, config.tileHeight, config.quantum);
        }

        std::unique_ptr<CoarseGrained> coarseGrained;
        if (config.mode == "coarse-grained") {
            coarseGrained = std::make_unique<CoarseGrained>(config.width, config.height,
                config.tileWidth, config.tileHeight, config.threads);
        }

        std::unique_ptr<Smt> smt;
        if (config.mode == "smt") {
            const int requestedThreads = config.threadsSpecified ? config.threads : 0;
            smt = std::make_unique<Smt>(config.width, config.height,
                config.tileWidth, config.tileHeight, requestedThreads);
        }

        std::unique_ptr<Cmp> cmp;
        if (config.mode == "cmp") {
            cmp = std::make_unique<Cmp>(config.width, config.height,
                config.tileWidth, config.tileHeight, config.threadsSpecified ? config.threads : 0);
        }

        // El reloj mide solo el kernel, excluyendo lectura e impresión.
        const auto start = std::chrono::steady_clock::now();
        if (cmp) {
            cmp->compute(a, b, result);
        } else if (smt) {
            smt->compute(a, b, result);
        } else if (coarseGrained) {
            coarseGrained->compute(a, b, result);
        } else if (fineGrained) {
            while (!fineGrained->finished()) {
                fineGrained->runRound(a, b, result);
            }
        } else if (config.mode == "tiled") {
            computeInterferogramTiled(a, b, result, config.width, tiles);
        } else {
            computeInterferogramSequential(a, b, result);
        }
        const auto stop = std::chrono::steady_clock::now();
        const double milliseconds =
            std::chrono::duration<double, std::milli>(stop - start).count();

        double exportMilliseconds = 0;
        if (!config.output.empty()) {
            const auto exportStart = std::chrono::steady_clock::now();
            exportResult(config.output, result, config);
            exportMilliseconds = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - exportStart).count();
        }

        // Resumen numérico para comparar ejecuciones, no una medida de calidad física.
        Complex sum{0, 0};
        std::size_t validSamples = 0;
        for (const auto& value : result) {
            if (!std::isfinite(value.real) || !std::isfinite(value.imag)) {
                continue;
            }
            ++validSamples;
            sum.real += value.real;
            sum.imag += value.imag;
        }
        std::cout << "P09-R2\n"
                  << "A: " << config.a << "\nB: " << config.b << '\n'
                  << "Window: " << config.width << " x " << config.height << '\n'
                  << "Offset: " << config.x << ", " << config.y << '\n'
                  << "Samples: " << a.size() << "\nMode: " << config.mode << '\n'
                  << std::fixed << std::setprecision(6)
                  << "Kernel time: " << milliseconds << " ms\n"
                  << "Export time: " << exportMilliseconds << " ms\n"
                  << std::setprecision(0)
                  << "Result sum (real, imag): " << sum.real << ", " << sum.imag << '\n'
                  << "Computational kernel; coregistration quality is not verified by this executable.\n";
        std::cout << "Result hash (fnv1a64): " << std::hex << resultFingerprint(result) << std::dec << '\n';
        std::cout << "Valid result samples: " << validSamples
                  << "\nInvalid result samples: " << result.size() - validSamples
                  << "\nNoData policy: " << (config.allowNoData ? "preserve; exclude nonfinite results from sum" : "reject") << '\n';
        if (config.mode == "tiled") {
            std::cout << "Tile size: " << config.tileWidth << " x " << config.tileHeight
                      << "\nTiles: " << tiles.size() << '\n';
        }
        if (fineGrained) {
            std::cout << "Execution: cooperative round-robin (1 OS thread)\n"
                      << "Tile size: " << config.tileWidth << " x " << config.tileHeight
                      << "\nTasks: " << fineGrained->tasks()
                      << "\nQuantum (pixels): " << config.quantum
                      << "\nRounds: " << fineGrained->rounds()
                      << "\nDispatches: " << fineGrained->dispatches() << '\n';
        }
        if (coarseGrained) {
            std::cout << "Execution: persistent OS threads, complete tiles\n"
                      << "Workers: " << coarseGrained->workers()
                      << "\nTiles: " << coarseGrained->tasks() << '\n';
        }
        if (smt) {
            const auto& topology = smt->topology();
            std::cout << "Execution: " << (smt->workers() > topology.physicalCores
                          ? "SMT approximation by thread oversubscription\n"
                          : "SMT control without oversubscription\n")
                      << "Workers: " << smt->workers()
                      << "\nTiles: " << smt->tasks()
                      << "\nAvailable physical cores: " << topology.physicalCores
                      << "\nAllowed logical CPUs: " << topology.allowedCpus.size()
                      << "\nHost SMT control: " << topology.smtControl
                      << "\nHost SMT active: " << topology.smtActive
                      << "\nAllowed CPU IDs:";
            for (int cpu : topology.allowedCpus) {
                std::cout << ' ' << cpu;
            }
            std::cout << "\nHardware SMT is not enabled or changed by this program.\n";
        }
        if (cmp) {
            std::cout << "Execution: CMP, one worker per physical core\n"
                      << "Workers: " << cmp->workers()
                      << "\nTiles: " << cmp->tasks() << "\nPinned CPU IDs:";
            for (int cpu : cmp->cpus()) {
                std::cout << ' ' << cpu;
            }
            std::cout << '\n';
        }
        return 0;
    // Convertir errores de configuración, lectura o cálculo en un fallo de la CLI.
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
