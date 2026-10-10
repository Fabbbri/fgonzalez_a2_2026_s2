#pragma once
#include "nbody/trajectory.hpp"

namespace nbody {
// Reproduce estados del archivo finalizado sin ejecutar el integrador.
// El cierre automático permite verificar controles con SDL dummy.
std::size_t replay_native(TrajectoryReader& trajectory, bool close_when_complete = false);
}  // namespace nbody
