/**
 * @file diagnostics.cpp
 * Diagnóstico físico de la interfaz secuencial anterior: masa, energía,
 * momento lineal y centro de masa. Es una lectura del estado y no altera
 * las aceleraciones. No forma parte de SOURCES del ejecutable multihilo actual.
 */
#include "nbody/diagnostics.hpp"
#include <cmath>
#include <stdexcept>

namespace nbody {
// Agrega magnitudes de una instantánea sin modificar partículas ni aceleraciones.
Diagnostics measure_physics(const std::vector<Body>& bodies) {
    // Acumuladores cero; center_of_mass empieza como numerador ponderado.
    Diagnostics result;
    // Recorrido de cuerpos para masa, energía cinética y momento.
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const Body& body = bodies[i];
        if (!std::isfinite(body.mass) || body.mass <= 0.0) {
            throw std::runtime_error("El diagnóstico requiere masas positivas y finitas.");
        }
        // Masa total: suma de masas positivas.
        result.mass += body.mass;
        // K_i=m_i*(vx²+vy²)/2, calculada con la velocidad de la instantánea.
        result.kinetic_energy += 0.5 * body.mass *
            (body.velocity.x * body.velocity.x + body.velocity.y * body.velocity.y);
        // Momento lineal total por componentes; no es momento angular.
        result.momentum.x += body.mass * body.velocity.x;
        result.momentum.y += body.mass * body.velocity.y;
        // Acumula numerador sum(m_i*r_i); se normaliza al terminar.
        result.center_of_mass.x += body.mass * body.position.x;
        result.center_of_mass.y += body.mass * body.position.y;

        // Cada pareja se cuenta una vez. El potencial usa el mismo suavizado
        // que las fuerzas: U_ij = -G*m_i*m_j / sqrt(r_ij² + epsilon²).
        // Potencial de pares únicos i<j: no duplica energía de una pareja ni incluye autoenergía.
        for (std::size_t j = i + 1; j < bodies.size(); ++j) {
            const double dx = bodies[j].position.x - body.position.x;
            const double dy = bodies[j].position.y - body.position.y;
            // Potencial gravitacional suavizado negativo con la misma epsilon de las fuerzas.
            result.potential_energy -= gravitational_constant * body.mass * bodies[j].mass /
                std::sqrt(dx * dx + dy * dy + softening * softening);
        }
    }
    if (result.mass == 0.0) {
        throw std::runtime_error("No hay cuerpos para calcular el centro de masa.");
    }
    // Normaliza el centro de masa por M, después de comprobar que no es cero.
    result.center_of_mass.x /= result.mass;
    result.center_of_mass.y /= result.mass;
    // Verifica que las magnitudes agregadas siguen siendo numéricamente representables.
    if (!std::isfinite(result.mass) || !std::isfinite(result.energy()) ||
        !std::isfinite(result.momentum.x) || !std::isfinite(result.momentum.y) ||
        !std::isfinite(result.center_of_mass.x) || !std::isfinite(result.center_of_mass.y)) {
        throw std::runtime_error("Diagnóstico físico no finito.");
    }
    return result;
}
}  // namespace nbody
