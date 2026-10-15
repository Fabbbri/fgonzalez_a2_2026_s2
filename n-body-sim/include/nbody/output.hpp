#pragma once
#include "nbody/diagnostics.hpp"
#include <iosfwd>

namespace nbody {
void write_physics_report(const Diagnostics& initial, const Diagnostics& final,
                          double time, std::ostream& output);
}  // namespace nbody
