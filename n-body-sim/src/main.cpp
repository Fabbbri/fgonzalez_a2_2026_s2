#include "nbody/config.hpp"
#include "nbody/native_view.hpp"
#include "nbody/simulation.hpp"
#include "nbody/trajectory.hpp"
#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char* argv[]) try {
    using namespace nbody;
    using Clock = std::chrono::steady_clock;
    if (argc == 2 && std::string(argv[1]) == "--help") {
        print_usage(std::cout);
        return 0;
    }
    const Config config = parse_arguments(argc, argv);
    if (!config.replay_path.empty()) {
        TrajectoryReader trajectory(config.replay_path);
        replay_native(trajectory);
        return 0;
    }
    auto bodies = create_galaxies(config.bodies_per_galaxy, config.seed);
    std::vector<Vec2> accelerations(bodies.size());
    // Intervalo total continuo: apertura, cálculo, captura, flush y cierre.
    // La generación aleatoria y la reproducción quedan fuera de este tiempo.
    const auto total_start = Clock::now();
    TrajectoryWriter writer(config.trajectory_path, config, bodies.size());
    refresh_accelerations(bodies, accelerations);
    writer.append(bodies);
    for (std::size_t step = 0; step < config.steps; ++step) {
        advance_one_step(bodies, accelerations, config.dt);
        writer.append(bodies);
    }
    writer.finish();
    const double total_seconds = std::chrono::duration<double>(Clock::now() - total_start).count();
    // Una cabecera y una fila por ejecución permiten agregación externa con Make/Bash.
    // Speedup, eficiencia y estadísticas requieren comparar varias muestras/modelos.
    std::cout << "model,workers,bodies,steps,dt,seed,total_seconds\n"
              << std::setprecision(17) << "sequential,1," << bodies.size() << ',' << config.steps
              << ',' << config.dt << ',' << config.seed << ',' << total_seconds << '\n';
    std::cout.flush();
    if (!std::cout) throw std::runtime_error("No se pudieron imprimir las métricas.");
    std::cerr << "Trayectoria finalizada: " << config.trajectory_path << '\n';
    std::cerr.flush();

    // La ventana solo se crea después de cerrar el archivo e imprimir los resultados.
    if (!config.no_output) {
        TrajectoryReader trajectory(config.trajectory_path);
        replay_native(trajectory);
    }
    return 0;
} catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << "\nUse --help para ver las opciones.\n";
    return 1;
}
