#pragma once
#include "nbody/simulation.hpp"

namespace nbody {
struct Diagnostics {
    double mass = 0.0;
    double kinetic_energy = 0.0;
    double potential_energy = 0.0;
    Vec2 momentum;
    Vec2 center_of_mass;

    double energy() const { return kinetic_energy + potential_energy; }
};

// Lectura del estado: no cambia las partículas ni sus aceleraciones.
Diagnostics measure_physics(const std::vector<Body>& bodies);
}  // namespace nbody
