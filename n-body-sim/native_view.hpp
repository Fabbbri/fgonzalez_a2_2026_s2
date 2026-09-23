#pragma once
#include "config.hpp"
#include "simulation.hpp"

namespace nbody {
struct RunResult {
    std::size_t completed_steps = 0;
    double compute_seconds = 0.0;
};

// Visualiza el mismo vector que modifica el integrador, sin archivos intermedios.
// El cierre automático permite verificar el ciclo gráfico en pruebas.
RunResult run_native(std::vector<Body>& bodies, std::vector<Vec2>& accelerations,
                     const Config& config, bool close_when_complete = false);
}  // namespace nbody
