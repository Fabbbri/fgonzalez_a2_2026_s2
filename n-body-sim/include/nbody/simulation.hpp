#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace nbody {
// Posiciones, velocidades y aceleraciones viven en el plano XY.
struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

struct Body {
    double mass;
    Vec2 position;
    Vec2 velocity;
    Vec2 acceleration;
    int galaxy_id;  // Conserva la galaxia de origen de cada cuerpo.
};


// Unidades normalizadas y suavizado compartido con el cálculo de energía.
inline constexpr double gravitational_constant = 1.0;
inline constexpr double softening = 0.05;

std::vector<Body> create_galaxies(std::size_t bodies_per_galaxy, std::uint32_t seed);
void compute_accelerations(const std::vector<Body>& bodies,
                           std::vector<Vec2>& accelerations,
                           double gravitational_constant, double softening);
void refresh_accelerations(std::vector<Body>& bodies, std::vector<Vec2>& accelerations);
// Requiere aceleraciones correspondientes a las posiciones actuales.
void advance_one_step(std::vector<Body>& bodies, std::vector<Vec2>& accelerations, double dt);
}  // namespace nbody
