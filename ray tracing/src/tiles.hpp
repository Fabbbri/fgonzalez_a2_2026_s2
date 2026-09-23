#pragma once

#include <cstddef>

namespace rt {
// Límites superiores exclusivos: también describen los tiles del borde.
struct Tile { int x0, y0, x1, y1; };
class TileGrid {
public:
    TileGrid(int width, int height, int tile_width, int tile_height);
    std::size_t size() const;
    Tile at(std::size_t index) const;
private:
    int width_, height_, tile_width_, tile_height_, columns_, rows_;
};
} // namespace rt
