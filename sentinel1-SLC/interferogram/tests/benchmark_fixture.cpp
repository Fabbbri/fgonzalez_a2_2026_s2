// TIFF solo para probar métricas, sin adquisiciones reales.
#include <gdal_priv.h>
#include <filesystem>
#include <stdexcept>
#include <vector>
#include <iostream>

/*
 * Crea A.tif y B.tif de 256 x 128 en DIRECTORIO, con una banda CFloat32.
 * test_benchmark.py lo ejecuta para medir campañas cortas sin depender de
 * los datos Sentinel reales.
 */
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("Uso: benchmark_fixture DIRECTORIO");
        std::filesystem::create_directories(argv[1]);
        // Registrar los formatos de GDAL y obtener el escritor de GeoTIFF.
        GDALAllRegister();
        auto* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
        if (!driver) throw std::runtime_error("Falta GTiff");
        // acquisition 0 genera A.tif y 1 genera B.tif.
        for (int acquisition = 0; acquisition < 2; ++acquisition) {
            const auto path = std::filesystem::path(argv[1]) / (acquisition ? "B.tif" : "A.tif");
            // Una banda compleja de floats, igual que los TIFF corregistrados.
            auto* dataset = driver->Create(path.c_str(), 256, 128, 1, GDT_CFloat32, nullptr);
            if (!dataset) throw std::runtime_error("No se pudo crear fixture");
            // Valores deterministas y distintos para A y B, sin NoData.
            // Pares (real, imaginario) por píxel, de ahí el * 2.
            std::vector<float> pixels(256 * 128 * 2);
            // Patrón periódico en [-7.875, 7.875]; el desfase de 5 hace que B difiera de A.
            for (size_t i = 0; i < pixels.size(); ++i)
                pixels[i] = static_cast<float>(static_cast<int>((i * 17 + acquisition * 5) % 127) - 63) / 8;
            // Escribir la imagen completa de una sola vez.
            const auto status = dataset->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, 256, 128,
                pixels.data(), 256, 128, GDT_CFloat32, 0, 0, nullptr);
            // Cerrar antes de revisar el estado para no dejar el archivo abierto.
            GDALClose(dataset);
            if (status != CE_None) throw std::runtime_error("Error escribiendo fixture");
        }
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
