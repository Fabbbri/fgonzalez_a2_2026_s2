#pragma once
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>

namespace nbody {
struct Config {
    std::size_t bodies_per_galaxy = 100;
    std::size_t steps = 10000;
    double dt = 0.002;
    std::uint32_t seed = 42;
    bool no_output = false;  // Guardar la simulación sin abrir el reproductor.
    std::string trajectory_path = "output/trajectory.nbody";
    std::string replay_path;
};

Config parse_arguments(int argc, char* argv[]);
void print_usage(std::ostream& output);
}  // namespace nbody
