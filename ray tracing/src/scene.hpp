#pragma once

#include "geometry.hpp"
#include <vector>
#include <cstdint>

namespace rt {
struct RenderSettings {
    int samples = 128;
    int max_bounces = 12;
    std::uint64_t seed = 2026;
};
struct Scene {
    std::vector<Sphere> spheres;
    // El cielo ilumina la escena: las superficies reciben luz de sus rebotes.
    Vec3 background{0.50, 0.70, 1.0};
    Vec3 horizon{1.0, 1.0, 1.0};
    RenderSettings settings;
};
Scene default_scene();
} // namespace rt
