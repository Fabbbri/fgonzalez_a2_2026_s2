/**
 * @file tiles.cpp
 * Conversión entre índice lineal de tile y región rectangular de imagen.
 * Con W por H y tiles Tw por Th, M = ceil(W/Tw) * ceil(H/Th).
 * No se almacena una lista de rectángulos: cada Tile se obtiene por aritmética.
 * La división entera identifica la fila y el residuo identifica la columna.
 */
#include "render/tiles.hpp"
#include <algorithm>
#include <stdexcept>

namespace rt {
TileGrid::TileGrid(int width, int height, int tw, int th)
    : width_(width), height_(height), tile_width_(tw), tile_height_(th) {
    // Dimensiones estrictamente positivas garantizan divisiones válidas y tiles no vacíos.
    if (width <= 0 || height <= 0 || tw <= 0 || th <= 0)
        throw std::invalid_argument("Dimensiones de tiles inválidas");
    // División redondeada hacia arriba: ceil(W/Tw), evitando sumar Tw a W.
    columns_ = 1 + (width - 1) / tw;
    // ceil(H/Th): incluye una última fila parcial si H no es múltiplo de Th.
    rows_ = 1 + (height - 1) / th;
}
// M = columnas por filas. size_t permite representar el número de regiones.
std::size_t TileGrid::size() const { return static_cast<std::size_t>(columns_) * rows_; }
Tile TileGrid::at(std::size_t index) const {
    // Valida que el índice global pertenece al dominio de trabajo [0,M).
    if (index >= size()) throw std::out_of_range("Índice de tile inválido");
    // Columna = index % columns_; su producto por Tw da el origen horizontal.
    const int x = static_cast<int>(index % columns_) * tile_width_;
    // Fila = index / columns_; su producto por Th da el origen vertical.
    const int y = static_cast<int>(index / columns_) * tile_height_;
    // El mínimo recorta el extremo derecho e inferior contra la imagen.
    // Con límites exclusivos, tiles adyacentes no comparten píxeles; esta
    // propiedad permite escritura concurrente sin un mutex por píxel.
    return {x, y, x + std::min(tile_width_, width_ - x),
                  y + std::min(tile_height_, height_ - y)};
}

} // namespace rt
