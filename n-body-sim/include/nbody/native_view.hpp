/**
 * @file native_view.hpp
 * Contrato de visualización integrada de la interfaz secuencial anterior.
 * RunResult registra pasos completados y tiempo acumulado de cálculo durante
 * el ciclo gráfico. La reproducción de archivos actual utiliza replay_native.
 */
#pragma once
#include "nbody/config.hpp"
#include "nbody/simulation.hpp"

namespace nbody {
// Resultado de la ejecución gráfica integrada anterior.
struct RunResult {
    // Cantidad de pasos físicos efectivamente integrados.
    std::size_t completed_steps = 0;
    // Tiempo acumulado de lotes de cálculo, excluyendo dibujo y espera de frames.
    double compute_seconds = 0.0;
};

// Visualiza el mismo vector que modifica el integrador, sin archivos intermedios.
// El cierre automático permite verificar el ciclo gráfico en pruebas.
// Modifica el estado durante eventos SDL con dt fijo; no es el reproductor actual de archivos.
RunResult run_native(std::vector<Body>& bodies, std::vector<Vec2>& accelerations,
                     const Config& config, bool close_when_complete = false);
}  // namespace nbody
