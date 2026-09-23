#pragma once
#include <cstddef>
#include <cstdint>
#include <iosfwd>

namespace nbody {
struct Config {
    std::size_t bodies_per_galaxy = 100;
    std::size_t steps = 10000;
    double dt = 0.002;
    std::uint32_t seed = 42;
    bool no_output = false;  // Ejecutar sin ventana para medir rendimiento.
};

Config parse_arguments(int argc, char* argv[]);
void print_usage(std::ostream& output);
}  // namespace nbody
