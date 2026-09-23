#include "config.hpp"
#include "diagnostics.hpp"
#include "native_view.hpp"
#include "output.hpp"
#include "simulation.hpp"
#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) try {
    using namespace nbody;
    if (argc == 2 && std::string(argv[1]) == "--help") {
        print_usage(std::cout);
        return 0;
    }
    const Config config = parse_arguments(argc, argv);
    auto bodies = create_galaxies(config.bodies_per_galaxy, config.seed);
    std::vector<Vec2> accelerations(bodies.size());

    // El diagnóstico O(N²) se calcula solo en los extremos, fuera del cronómetro.
    const Diagnostics initial = measure_physics(bodies);
    RunResult result;
    if (config.no_output) {
        // SDL no se inicializa aquí. Se mide un intervalo continuo de cálculo.
        using Clock = std::chrono::steady_clock;
        const auto start = Clock::now();
        refresh_accelerations(bodies, accelerations);
        for (std::size_t index = 0; index < config.steps; ++index) {
            advance_one_step(bodies, accelerations, config.dt);
        }
        result.compute_seconds = std::chrono::duration<double>(Clock::now() - start).count();
        result.completed_steps = config.steps;
    } else {
        result = run_native(bodies, accelerations, config);
    }
    const Diagnostics final = measure_physics(bodies);
    const double time = static_cast<double>(result.completed_steps) * config.dt;
    std::cerr << std::setprecision(10)
              << "Cuerpos: " << bodies.size() << ", pasos completados: " << result.completed_steps << '/' << config.steps
              << ", dt: " << config.dt << ", semilla: " << config.seed << '\n'
              << "Modo: " << (config.no_output ? "sin ventana" : "ventana nativa") << '\n'
              << "Tiempo simulado: " << time << '\n'
              << "Tiempo de cálculo secuencial (sin inicialización aleatoria, diagnóstico ni dibujo): "
              << result.compute_seconds << " s\n";
    if (!config.no_output) {
        std::cerr << "Para benchmarking use --no-output: el modo visual incluye pausas entre cálculos.\n";
    }
    write_physics_report(initial, final, time, std::cerr);
    return 0;
} catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << "\nUse --help para ver las opciones.\n";
    return 1;
}
