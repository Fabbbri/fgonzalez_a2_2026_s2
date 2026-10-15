// TIFF sintéticos solo para probar métricas, sin adquisiciones reales.
#include <gdal_priv.h>
#include <filesystem>
#include <stdexcept>
#include <vector>
#include <iostream>
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("Uso: benchmark_fixture DIRECTORIO");
        std::filesystem::create_directories(argv[1]);
        GDALAllRegister();
        auto* driver = GetGDALDriverManager()->GetDriverByName("GTiff");
        if (!driver) throw std::runtime_error("Falta GTiff");
        for (int acquisition = 0; acquisition < 2; ++acquisition) {
            const auto path = std::filesystem::path(argv[1]) / (acquisition ? "B.tif" : "A.tif");
            auto* dataset = driver->Create(path.c_str(), 256, 128, 1, GDT_CFloat32, nullptr);
            if (!dataset) throw std::runtime_error("No se pudo crear fixture");
            std::vector<float> pixels(256 * 128 * 2);
            for (size_t i = 0; i < pixels.size(); ++i)
                pixels[i] = static_cast<float>(static_cast<int>((i * 17 + acquisition * 5) % 127) - 63) / 8;
            const auto status = dataset->GetRasterBand(1)->RasterIO(GF_Write, 0, 0, 256, 128,
                pixels.data(), 256, 128, GDT_CFloat32, 0, 0, nullptr);
            GDALClose(dataset);
            if (status != CE_None) throw std::runtime_error("Error escribiendo fixture");
        }
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
