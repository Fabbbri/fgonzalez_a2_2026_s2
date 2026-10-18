#pragma once

#include "scene/camera.hpp"
#include "render/image.hpp"
#include "scene/scene.hpp"
#include "render/tiles.hpp"
#include <functional>

namespace rt {
Pixel render_pixel(const Scene& scene, const Camera& camera, int x, int y);
// Estado de una tarea de tile. next cuenta píxeles COMPLETOS, no rayos.
struct TileProgress {
    Tile tile;
    std::size_t next = 0;
};
std::size_t tile_pixels(Tile tile);
// Avanza como máximo budget píxeles; false significa pendiente o cancelado.
bool render_tile_step(const Scene& scene, const Camera& camera, Image& image,
                      TileProgress& progress, std::size_t budget,
                      const std::function<bool()>& keep_running = {});
// El callback opcional solo atiende cancelación; nunca cambia de tile.
bool render_tile(const Scene& scene, const Camera& camera, Image& image, Tile tile,
                 const std::function<bool()>& keep_running = {});
} // namespace rt
