/**
 * @file output.hpp
 * Interfaz de salida del diagnóstico físico anterior.
 * Recibe estados agregados y escribe diferencias sin modificar la simulación.
 */
#pragma once
#include "nbody/diagnostics.hpp"
#include <iosfwd>

namespace nbody {
// Compara diagnósticos inicial/final y escribe en un stream externo sin alterar datos.
void write_physics_report(const Diagnostics& initial, const Diagnostics& final,
                          double time, std::ostream& output);
}  // namespace nbody
