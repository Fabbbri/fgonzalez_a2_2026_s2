#include "interferogram/config.hpp"
#include "interferogram/slc_reader.hpp"
#include "interferogram/interferogram.hpp"
#include "interferogram/models/fine_grained.hpp"
#include "interferogram/models/coarse_grained.hpp"
#include "interferogram/models/smt.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <memory>

/*
 * Coordina la configuración, la lectura de las adquisiciones y el cálculo.
 *
 * @param argc Número de argumentos recibidos.
 * @param argv Arreglo de argumentos de la línea de comandos.
 * @return 0 al completar el cálculo o mostrar ayuda; 1 ante un error capturado.
 */
int main(int argc, char* argv[]) {
    using namespace interferogram;
    try {
        const Config config = parseConfig(argc, argv);
        if (config.help) {
            printHelp(std::cout);
            return 0;
        }
        // Usar las mismas coordenadas en A y B; esto no realiza corregistro.
        const auto a = readSlcWindow(config.a, config.x, config.y, config.width, config.height);
        const auto b = readSlcWindow(config.b, config.x, config.y, config.width, config.height);
        // Preparar y validar la salida antes de iniciar el cronómetro.
        std::vector<Complex> result(a.size());
        validateInterferogramInputs(a, b, result);

        // Preparar la partición fuera de la medición del kernel.
        const auto tiles = config.mode == "tiled"
            ? makeTiles(config.width, config.height, config.tileWidth, config.tileHeight)
            : std::vector<Tile>{};

        std::unique_ptr<FineGrained> fineGrained;
        if (config.mode == "fine-grained")
            fineGrained = std::make_unique<FineGrained>(config.width, config.height,
                config.tileWidth, config.tileHeight, config.quantum);

        std::unique_ptr<CoarseGrained> coarseGrained;
        if (config.mode == "coarse-grained")
            coarseGrained = std::make_unique<CoarseGrained>(config.width, config.height,
                config.tileWidth, config.tileHeight, config.threads);

        std::unique_ptr<Smt> smt;
        if (config.mode == "smt") {
            const int requestedThreads = config.threadsSpecified ? config.threads : 0;
            smt = std::make_unique<Smt>(config.width, config.height,
                config.tileWidth, config.tileHeight, requestedThreads);
        }

        // El reloj mide solo el kernel, excluyendo lectura e impresión.
        const auto start = std::chrono::steady_clock::now();
        if (smt) {
            smt->compute(a, b, result);
        } else if (coarseGrained) {
            coarseGrained->compute(a, b, result);
        } else if (fineGrained) {
            while (!fineGrained->finished())
                fineGrained->runRound(a, b, result);
        } else if (config.mode == "tiled")
            computeInterferogramTiled(a, b, result, config.width, tiles);
        else
            computeInterferogramSequential(a, b, result);
        const auto stop = std::chrono::steady_clock::now();
        const double milliseconds =
            std::chrono::duration<double, std::milli>(stop - start).count();

        // Resumen numérico para comparar ejecuciones, no una medida de calidad física.
        Complex sum{0, 0};
        for (const auto& value : result) {
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
                  << std::setprecision(0)
                  << "Result sum (real, imag): " << sum.real << ", " << sum.imag << '\n'
                  << "Computational kernel; coregistration quality is not verified by this executable.\n";
        if (config.mode == "tiled")
            std::cout << "Tile size: " << config.tileWidth << " x " << config.tileHeight
                      << "\nTiles: " << tiles.size() << '\n';
        if (fineGrained)
            std::cout << "Execution: cooperative round-robin (1 OS thread)\n"
                      << "Tile size: " << config.tileWidth << " x " << config.tileHeight
                      << "\nTasks: " << fineGrained->tasks()
                      << "\nQuantum (pixels): " << config.quantum
                      << "\nRounds: " << fineGrained->rounds()
                      << "\nDispatches: " << fineGrained->dispatches() << '\n';
        if (coarseGrained)
            std::cout << "Execution: persistent OS threads, complete tiles\n"
                      << "Workers: " << coarseGrained->workers()
                      << "\nTiles: " << coarseGrained->tasks() << '\n';
        if (smt) {
            const auto& topology = smt->topology();
            std::cout << "Execution: SMT approximation by thread oversubscription\n"
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
        return 0;
    // Convertir errores de configuración, lectura o cálculo en un fallo de la CLI.
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
