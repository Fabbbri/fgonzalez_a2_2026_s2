#pragma once

#include "app/options.hpp"

namespace rt {
void benchmark(const Options& options, const Scene& scene, const Camera& camera,
               const TileGrid& tiles);
} // namespace rt
