/**
 * @file benchmark.hpp
 * Interfaz de la campaña de mediciones sin ventana.
 * La escena, cámara y partición llegan construidas; las mediciones se exportan
 * en CSV crudo y resumen por configuración.
 */
#pragma once

#include "app/options.hpp"

namespace rt {
// Ejecuta calentamientos y repeticiones para cada P, verifica imagen y exporta CSV.
// La carga se recibe construida y se conserva entre modelos.
void benchmark(const Options& options, const Scene& scene, const Camera& camera,
               const TileGrid& tiles);
} // namespace rt
