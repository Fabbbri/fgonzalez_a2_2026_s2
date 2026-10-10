#include "nbody.hpp"
#include <charconv>
#include <cmath>
#include <limits>
#include <ostream>
#include <chrono>
#include <exception>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string>

namespace nbody {
std::size_t parse_integer(const std::string& value) {
    std::size_t result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
        throw std::invalid_argument("Entero no válido: " + value);
    }
    return result;
}

// Validamos los parámetros antes de reservar memoria o crear archivos.
Config parse_arguments(int argc, char* argv[]) {
    Config config;
    for (int i = 1; i < argc; ++i) {
        const std::string option = argv[i];
        if (option == "--no-output") {
            config.no_output = true;
            continue;
        }
        if (option != "--bodies-per-galaxy" && option != "--steps" &&
            option != "--dt" && option != "--seed" &&
            option != "--trajectory" && option != "--replay") {
            throw std::invalid_argument("Opción desconocida: " + option);
        }
        if (++i == argc) {
            throw std::invalid_argument("Falta el valor de " + option);
        }
        const std::string value = argv[i];
        if (option == "--trajectory" || option == "--replay") {
            if (value.empty()) throw std::invalid_argument("La ruta no puede estar vacía.");
            if (option == "--trajectory") config.trajectory_path = value;
            else config.replay_path = value;
        } else if (option == "--bodies-per-galaxy") {
            config.bodies_per_galaxy = parse_integer(value);
        } else if (option == "--steps") {
            config.steps = parse_integer(value);
        } else if (option == "--seed") {
            const std::size_t seed = parse_integer(value);
            if (seed > std::numeric_limits<std::uint32_t>::max()) {
                throw std::invalid_argument("La semilla debe caber en 32 bits.");
            }
            config.seed = static_cast<std::uint32_t>(seed);
        } else {
            std::size_t consumed = 0;
            config.dt = std::stod(value, &consumed);
            if (consumed != value.size()) {
                throw std::invalid_argument("dt no válido: " + value);
            }
        }
    }
    if (!config.replay_path.empty() && argc != 3) {
        throw std::invalid_argument("Use --replay ARCHIVO sin opciones de simulación.");
    }
    if (config.steps == std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument("Demasiados pasos para guardar el estado inicial y final.");
    }
    if (config.bodies_per_galaxy == 0) {
        throw std::invalid_argument("La cantidad de cuerpos debe ser positiva.");
    }
    if (!std::isfinite(config.dt) || config.dt <= 0.0 ||
        !std::isfinite(config.dt * static_cast<double>(config.steps))) {
        throw std::invalid_argument("dt debe ser positivo y el tiempo final debe ser finito.");
    }
    return config;
}


void print_usage(std::ostream& output) {
    output << "Uso: nbody [opciones]\n"
              "  --bodies-per-galaxy N  Cuerpos por galaxia (100)\n"
              "  --steps N              Pasos; 0 = estado inicial (10000)\n"
              "  --dt X                 Duración de cada paso (0.002)\n"
              "  --seed N               Semilla de 32 bits (42)\n"
              "  --trajectory ARCHIVO   Trayectoria (output/trajectory.nbody)\n"
              "  --no-output            Guardar y medir sin abrir la ventana\n"
              "  --replay ARCHIVO       Reproducir una trayectoria sin simular\n"
              "Simula, cierra la trayectoria, imprime métricas CSV y abre el reproductor.\n"
              "Espacio: pausa; N: un frame; R: volver al inicio; F: encuadrar.\n"
              "Rueda: zoom; arrastrar: mover cámara; +/-: velocidad; Esc: salir.\n"
              "T: estelas; G: rejilla; flechas o clic en la barra: buscar y pausar.\n"
              "Tiempo total y parámetros en CSV por stdout, antes de visualizar.\n";
}
}  // namespace nbody

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
    auto model = make_sequential_model();
    const std::string model_name = model->name();
    const std::size_t workers = model->workers();
    TrajectoryWriter writer(config.trajectory_path, config, bodies.size());
    refresh_accelerations(bodies, accelerations, *model);
    writer.append(bodies);
    for (std::size_t step = 0; step < config.steps; ++step) {
        advance_one_step(bodies, accelerations, config.dt, *model);
        writer.append(bodies);
    }
    model.reset();  // Incluye el cierre de futuros trabajadores en el tiempo total.
    writer.finish();
    const double total_seconds = std::chrono::duration<double>(Clock::now() - total_start).count();
    // Una cabecera y una fila por ejecución permiten agregación externa con Make/Bash.
    // Speedup, eficiencia y estadísticas requieren comparar varias muestras/modelos.
    std::cout << "model,workers,bodies,steps,dt,seed,total_seconds\n"
              << std::setprecision(17) << model_name << ',' << workers << ',' << bodies.size() << ',' << config.steps
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
