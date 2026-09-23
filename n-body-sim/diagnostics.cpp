#include "diagnostics.hpp"
#include <cmath>
#include <stdexcept>

namespace nbody {
Diagnostics measure_physics(const std::vector<Body>& bodies) {
    Diagnostics result;
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const Body& body = bodies[i];
        if (!std::isfinite(body.mass) || body.mass <= 0.0) {
            throw std::runtime_error("El diagnóstico requiere masas positivas y finitas.");
        }
        result.mass += body.mass;
        result.kinetic_energy += 0.5 * body.mass *
            (body.velocity.x * body.velocity.x + body.velocity.y * body.velocity.y);
        result.momentum.x += body.mass * body.velocity.x;
        result.momentum.y += body.mass * body.velocity.y;
        result.center_of_mass.x += body.mass * body.position.x;
        result.center_of_mass.y += body.mass * body.position.y;

        // Cada pareja se cuenta una vez. El potencial usa el mismo suavizado
        // que las fuerzas: U_ij = -G*m_i*m_j / sqrt(r_ij² + epsilon²).
        for (std::size_t j = i + 1; j < bodies.size(); ++j) {
            const double dx = bodies[j].position.x - body.position.x;
            const double dy = bodies[j].position.y - body.position.y;
            result.potential_energy -= gravitational_constant * body.mass * bodies[j].mass /
                std::sqrt(dx * dx + dy * dy + softening * softening);
        }
    }
    if (result.mass == 0.0) {
        throw std::runtime_error("No hay cuerpos para calcular el centro de masa.");
    }
    result.center_of_mass.x /= result.mass;
    result.center_of_mass.y /= result.mass;
    if (!std::isfinite(result.mass) || !std::isfinite(result.energy()) ||
        !std::isfinite(result.momentum.x) || !std::isfinite(result.momentum.y) ||
        !std::isfinite(result.center_of_mass.x) || !std::isfinite(result.center_of_mass.y)) {
        throw std::runtime_error("Diagnóstico físico no finito.");
    }
    return result;
}
}  // namespace nbody
