#pragma once

#include "camera.hpp"
#include "image.hpp"
#include "scene.hpp"
#include "tiles.hpp"
#include <functional>

namespace rt {
Pixel render_pixel(const Scene& scene, const Camera& camera, int x, int y);
// El callback opcional solo atiende cancelación; nunca cambia de tile.
bool render_tile(const Scene& scene, const Camera& camera, Image& image, Tile tile,
                 const std::function<bool()>& keep_running = {});
} // namespace rt
