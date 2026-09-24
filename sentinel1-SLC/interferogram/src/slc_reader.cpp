#include "interferogram/slc_reader.hpp"
#include <gdal_priv.h>
#include <cpl_error.h>
#include <memory>
#include <stdexcept>

namespace interferogram {
void validateWindow(int x, int y, int width, int height) {
    // Subtraction avoids overflow for large CLI arguments.
    if (x < 0 || y < 0 || width <= 0 || height <= 0 ||
        width > slcWidth || height > slcHeight ||
        x > slcWidth - width || y > slcHeight - height)
        throw std::invalid_argument("Ventana fuera de límites: se requiere 24445 x 1500.");
}
std::vector<Complex> readSlcWindow(const std::string& path,
                                 int x, int y, int width, int height) {
    validateWindow(x, y, width, height);
    GDALAllRegister();
    using Dataset = std::unique_ptr<GDALDataset, decltype(&GDALClose)>;
    Dataset dataset(static_cast<GDALDataset*>(GDALOpen(path.c_str(), GA_ReadOnly)),
                    GDALClose);
    if (!dataset)
        throw std::runtime_error("No se pudo abrir " + path + ": " + CPLGetLastErrorMsg());
    if (dataset->GetRasterCount() != 1 ||
        dataset->GetRasterXSize() != slcWidth ||
        dataset->GetRasterYSize() != slcHeight)
        throw std::runtime_error("Se esperaba una banda de 24445 x 1500: " + path);
    GDALRasterBand* band = dataset->GetRasterBand(1);
    if (!band || !GDALDataTypeIsComplex(band->GetRasterDataType()))
        throw std::runtime_error("Se esperaba una banda compleja: " + path);
    std::vector<Complex> result(static_cast<std::size_t>(width) * height);
    // Read one row at a time, preserving real/imaginary components and avoiding
    // assumptions about the padding or memory layout of Complex.
    std::vector<double> row(static_cast<std::size_t>(width) * 2);
    for (int j = 0; j < height; ++j) {
        if (band->RasterIO(GF_Read, x, y + j, width, 1, row.data(),
                           width, 1, GDT_CFloat64, 0, 0, nullptr) != CE_None)
            throw std::runtime_error("Error de lectura en " + path + ": " + CPLGetLastErrorMsg());
        for (int i = 0; i < width; ++i)
            result[static_cast<std::size_t>(j) * width + i] =
                {row[2 * i], row[2 * i + 1]};
    }
    return result;
}
} // namespace interferogram
