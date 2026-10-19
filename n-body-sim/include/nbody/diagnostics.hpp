/**
 * @file diagnostics.hpp
 * Magnitudes físicas agregadas de la interfaz secuencial anterior.
 * La energía total es cinética más potencial; las demás magnitudes permiten
 * comprobar conservación y movimiento global de la configuración.
 */
#pragma once
#include "nbody/simulation.hpp"

namespace nbody {
// Acumulados físicos de una instantánea para estudiar comportamiento numérico.
struct Diagnostics {
    // Masa total M=sum_i m_i.
    double mass = 0.0;
    // Energía cinética K=sum_i m_i*|v_i|²/2.
    double kinetic_energy = 0.0;
    // Potencial U de pares únicos con la misma regularización de la fuerza.
    double potential_energy = 0.0;
    // Momento lineal total P=sum_i m_i*v_i.
    Vec2 momentum;
    // Centro R=sum_i m_i*r_i/M.
    Vec2 center_of_mass;

    // Energía mecánica total E=K+U, consultada sin modificar diagnóstico.
    double energy() const { return kinetic_energy + potential_energy; }
};

// Lectura del estado: no cambia las partículas ni sus aceleraciones.
// Lectura O(N²) por el potencial, independiente del integrador.
Diagnostics measure_physics(const std::vector<Body>& bodies);
}  // namespace nbody
