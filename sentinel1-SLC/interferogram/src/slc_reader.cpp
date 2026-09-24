#include "interferogram/slc_reader.hpp"
#include <gdal_priv.h>
#include <cpl_error.h>
#include <memory>
#include <stdexcept>

namespace interferogram {
/*
 * Comprueba que la ventana esté contenida en el burst esperado para P09-R2.
 *
 * @param x      Columna inicial de la ventana, en base cero.
 * @param y      Fila inicial de la ventana, en base cero.
 * @param width  Ancho de la ventana en píxeles; debe ser positivo.
 * @param height Alto de la ventana en píxeles; debe ser positivo.
 *
 * @throws std::invalid_argument si el origen es negativo, el tamaño no es
 *         positivo o la ventana excede las dimensiones slcWidth × slcHeight.
 */
void validateWindow(int x, int y, int width, int height) {
    // Restar dimensiones evita el desbordamiento que podría producir x + width
    // o y + height con argumentos grandes; primero se comprueban los tamaños.
    if (x < 0 || y < 0 || width <= 0 || height <= 0 ||
        width > slcWidth || height > slcHeight ||
        x > slcWidth - width || y > slcHeight - height)
        throw std::invalid_argument("Ventana fuera de límites: se requiere 24445 x 1500.");
}
/*
 * Abre una adquisición en modo lectura y extrae una ventana compleja.
 *
 * Exige una banda compleja de dimensiones slcWidth × slcHeight. Conserva
 * las componentes real e imaginaria sin realizar remuestreo ni corregistro.
 *
 * @param path   Ruta del archivo de la adquisición SLC.
 * @param x      Columna inicial de la ventana, en base cero.
 * @param y      Fila inicial de la ventana, en base cero.
 * @param width  Ancho de la ventana en píxeles.
 * @param height Alto de la ventana en píxeles.
 * @return Vector de width * height muestras ordenadas por filas:
 *         índice = fila * width + columna, con coordenadas locales a la ventana.
 *
 * @throws std::invalid_argument si la ventana no cumple los límites esperados.
 * @throws std::runtime_error si falla la apertura o lectura, o si el archivo
 *         tiene dimensiones, cantidad de bandas o tipo de dato incompatibles.
 */
std::vector<Complex> readSlcWindow(const std::string& path,
                                 int x, int y, int width, int height) {
    validateWindow(x, y, width, height);
    // Registrar los controladores de formatos antes de intentar abrir el TIFF.
    GDALAllRegister();
    // El puntero libera el archivo con GDALClose, incluso al lanzar una excepción.
    using Dataset = std::unique_ptr<GDALDataset, decltype(&GDALClose)>;
    Dataset dataset(static_cast<GDALDataset*>(GDALOpen(path.c_str(), GA_ReadOnly)),
                    GDALClose);
    if (!dataset)
        throw std::runtime_error("No se pudo abrir " + path + ": " + CPLGetLastErrorMsg());
    if (dataset->GetRasterCount() != 1 ||
        dataset->GetRasterXSize() != slcWidth ||
        dataset->GetRasterYSize() != slcHeight)
        throw std::runtime_error("Se esperaba una banda de 24445 x 1500: " + path);
    // GDAL numera las bandas desde 1; se exige un tipo que conserve la fase.
    GDALRasterBand* band = dataset->GetRasterBand(1);
    if (!band || !GDALDataTypeIsComplex(band->GetRasterDataType()))
        throw std::runtime_error("Se esperaba una banda compleja: " + path);
    std::vector<Complex> result(static_cast<std::size_t>(width) * height);
    // GDT_CFloat64 entrega pares de doubles: real, imaginario, real, imaginario...
    // El búfer por fila evita depender de la distribución en memoria de Complex.
    std::vector<double> row(static_cast<std::size_t>(width) * 2);
    for (int j = 0; j < height; ++j) {
        // Leer una fila sin remuestreo: las dimensiones de entrada y salida coinciden.
        if (band->RasterIO(GF_Read, x, y + j, width, 1, row.data(),
                           width, 1, GDT_CFloat64, 0, 0, nullptr) != CE_None)
            throw std::runtime_error("Error de lectura en " + path + ": " + CPLGetLastErrorMsg());
        // Convertir la posición local (j, i) al índice lineal j * width + i.
        for (int i = 0; i < width; ++i)
            result[static_cast<std::size_t>(j) * width + i] =
                {row[2 * i], row[2 * i + 1]};
    }
    return result;
}
} 
