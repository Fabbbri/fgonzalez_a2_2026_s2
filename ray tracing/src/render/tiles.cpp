#include "render/tiles.hpp"
#include <algorithm>
#include <stdexcept>

namespace rt {
TileGrid::TileGrid(int width, int height, int tw, int th)
    : width_(width), height_(height), tile_width_(tw), tile_height_(th) {
    if (width <= 0 || height <= 0 || tw <= 0 || th <= 0)
        throw std::invalid_argument("Dimensiones de tiles inválidas");
    columns_ = 1 + (width - 1) / tw;
    rows_ = 1 + (height - 1) / th;
}
std::size_t TileGrid::size() const { return static_cast<std::size_t>(columns_) * rows_; }
Tile TileGrid::at(std::size_t index) const {
    if (index >= size()) throw std::out_of_range("Índice de tile inválido");
    const int x = static_cast<int>(index % columns_) * tile_width_;
    const int y = static_cast<int>(index / columns_) * tile_height_;
    return {x, y, x + std::min(tile_width_, width_ - x),
                  y + std::min(tile_height_, height_ - y)};
}

} // namespace rt
