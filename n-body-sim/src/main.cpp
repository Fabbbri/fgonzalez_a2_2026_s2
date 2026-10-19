/**
 * @file main.cpp
 * Entrada del ejecutable multihilo: argumentos, simulación, trayectoria y métricas.
 * Construye el modelo una vez, ejecuta fases y conserva la misma física para
 * todos los esquemas. El tiempo continuo incluye grupo de hilos, captura y
 * cierre; generación inicial, salida CSV y reproducción SDL quedan fuera.
 */
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
// Convierte la cadena completa a entero de tamaño, rechazando sufijos y desbordamientos.
std::size_t parse_integer(const std::string& value) {
    std::size_t result = 0;
    // from_chars separa error de conversión y posición final sin depender de la región del sistema.
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
        throw std::invalid_argument("Entero no válido: " + value);
    }
    return result;
}

// Validamos los parámetros antes de reservar memoria o crear archivos.
// Construye configuración validada a partir de argv; argv[0] es el nombre del programa.
Config parse_arguments(int argc, char* argv[]) {
    // Parte de los valores predeterminados de la cabecera correspondiente a esta ruta.
    Config config;
    // Registra opciones explícitas para distinguir omisión de parámetros incompatibles.
    bool quantum_given = false;
    // Permite asignar P automático a SMT/CMP solo si --threads no se proporcionó.
    bool threads_given = false;
    // Las pausas temporizadas pertenecen solo a coarse; cero sigue siendo una elección explícita.
    bool block_time_given = false;
    // Recorre banderas; las que llevan valor consumen además el siguiente argumento.
    for (int i = 1; i < argc; ++i) {
        // Copia la bandera actual para comparación segura de cadenas.
        const std::string option = argv[i];
        // Bandera sin valor que configura el modo de presentación de esta interfaz.
        if (option == "--no-output") {
            config.no_output = true;
            continue;
        }
        // Lista de opciones reconocidas; cualquier nombre ajeno produce error.
        if (option != "--bodies-per-galaxy" && option != "--steps" &&
            option != "--dt" && option != "--seed" &&
            option != "--trajectory" && option != "--replay" &&
            option != "--model" && option != "--quantum" && option != "--threads" &&
            option != "--block-interval-ms" && option != "--block-duration-ms") {
            throw std::invalid_argument("Opción desconocida: " + option);
        }
        // Consume argumento de valor y detecta una opción final sin valor.
        if (++i == argc) {
            throw std::invalid_argument("Falta el valor de " + option);
        }
        const std::string value = argv[i];
        if (option == "--model") {
            config.model = value;
        } else if (option == "--block-interval-ms" || option == "--block-duration-ms") {
            // Duraciones enteras convertidas al rango de int usado por chrono.
            const auto milliseconds = parse_integer(value);
            if (milliseconds > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
                throw std::invalid_argument("Tiempo de bloqueo fuera de rango.");
            }
            if (option == "--block-interval-ms") config.block_interval_ms = static_cast<int>(milliseconds);
            else config.block_duration_ms = static_cast<int>(milliseconds);
            block_time_given = true;
        } else if (option == "--threads") {
            config.threads = parse_integer(value);
            threads_given = true;
        } else if (option == "--quantum") {
            config.quantum = parse_integer(value);
            quantum_given = true;
        } else if (option == "--trajectory" || option == "--replay") {
            if (value.empty()) throw std::invalid_argument("La ruta no puede estar vacía.");
            if (option == "--trajectory") config.trajectory_path = value;
            else config.replay_path = value;
        } else if (option == "--bodies-per-galaxy") {
            config.bodies_per_galaxy = parse_integer(value);
        } else if (option == "--steps") {
            config.steps = parse_integer(value);
        } else if (option == "--seed") {
            // Valida semilla en el tipo de tamaño antes de restringirla a 32 bits.
            const std::size_t seed = parse_integer(value);
            if (seed > std::numeric_limits<std::uint32_t>::max()) {
                throw std::invalid_argument("La semilla debe caber en 32 bits.");
            }
            config.seed = static_cast<std::uint32_t>(seed);
        } else {
            // Cuenta caracteres consumidos por stod para rechazar texto sobrante en dt.
            std::size_t consumed = 0;
            config.dt = std::stod(value, &consumed);
            if (consumed != value.size()) {
                throw std::invalid_argument("dt no válido: " + value);
            }
        }
    }
    // Reproducción es un modo exclusivo: solo acepta la ruta del archivo.
    if (!config.replay_path.empty() && argc != 3) {
        throw std::invalid_argument("Use --replay ARCHIVO sin opciones de simulación.");
    }
    // La estrategia debe tener una implementación de fábrica disponible.
    if (config.model != "sequential" && config.model != "fine-grained" &&
        config.model != "coarse-grained" && config.model != "smt" && config.model != "cmp") {
        throw std::invalid_argument("Modelo no disponible: " + config.model);
    }
    // Quantum es positivo y solo se admite explícitamente con fine.
    if (config.quantum == 0 || (quantum_given && config.model != "fine-grained")) {
        throw std::invalid_argument("--quantum debe ser positivo y requiere --model fine-grained.");
    }
    // P explícito positivo pertenece a coarse, SMT o CMP.
    if (config.threads == 0 || (threads_given && config.model != "coarse-grained" &&
                              config.model != "smt" && config.model != "cmp")) {
        throw std::invalid_argument("--threads debe ser positivo y requiere coarse-grained, smt o cmp.");
    }
    // Cero posterior a validación es el centinela interno para L o 2*C automáticos.
    if (config.model == "smt" && !threads_given) config.threads = 0;  // Automático: L o 2*C sin siblings.
    if (block_time_given && config.model != "coarse-grained") {
        throw std::invalid_argument("Los tiempos de bloqueo requieren --model coarse-grained.");
    }
    if (config.model == "cmp" && !threads_given) config.threads = 0;  // Automático: C.
    // Protege la suma steps+1 usada para incluir el estado inicial en la trayectoria.
    if (config.steps == std::numeric_limits<std::size_t>::max()) {
        throw std::invalid_argument("Demasiados pasos para guardar el estado inicial y final.");
    }
    // La distribución inicial y las fórmulas de masa requieren población positiva.
    if (config.bodies_per_galaxy == 0) {
        throw std::invalid_argument("La cantidad de cuerpos debe ser positiva.");
    }
    // dt positivo y tiempo total finito evitan parámetros físicamente indefinidos.
    if (!std::isfinite(config.dt) || config.dt <= 0.0 ||
        !std::isfinite(config.dt * static_cast<double>(config.steps))) {
        throw std::invalid_argument("dt debe ser positivo y el tiempo final debe ser finito.");
    }
    return config;
}


// Ayuda específica de esta interfaz; no crea recursos ni ejecuta física.
void print_usage(std::ostream& output) {
    output << "Uso: nbody [opciones]\n"
              "  --model MODELO         sequential (defecto), fine-grained, coarse-grained, smt o cmp\n"
              "  --quantum N            Interacciones por turno Fine-Grained (1)\n"
              "  --threads N            Coarse: 2; SMT: L (o 2*C sin siblings); CMP: C\n"
              "  --block-interval-ms N  Coarse: intervalo entre bloqueos (10); 0 los desactiva\n"
              "  --block-duration-ms N  Coarse: duración de cada espera (1)\n"
              "  --topology             Consultar núcleos disponibles y estado SMT, sin simular\n"
              "  --defaults             Consultar los valores de Config en JSON, sin simular\n"
              "  --bodies-per-galaxy N  Cuerpos por galaxia (500)\n"
              "  --steps N              Pasos; 0 = estado inicial (1000)\n"
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

// Entrada con function-try-block: cualquier excepción del flujo se convierte en diagnóstico y salida 1.
int main(int argc, char* argv[]) try {
    using namespace nbody;
    // Reloj monotónico del intervalo continuo de simulación y captura.
    using Clock = std::chrono::steady_clock;
    // Consultas exclusivas de ayuda, topología y valores por defecto terminan sin simular.
    if (argc == 2 && std::string(argv[1]) == "--help") {
        print_usage(std::cout);
        return 0;
    }
    if (argc == 2 && std::string(argv[1]) == "--topology") {
        print_cpu_topology(std::cout, detect_cpu_topology());
        return 0;
    }
    // Emite JSON desde Config real; el script de benchmark evita duplicar valores predeterminados.
    if (argc == 2 && std::string(argv[1]) == "--defaults") {
        const Config defaults;
        std::cout << std::setprecision(17)
                  << "{\"bodies_per_galaxy\":" << defaults.bodies_per_galaxy
                  << ",\"steps\":" << defaults.steps
                  << ",\"dt\":" << defaults.dt
                  << ",\"seed\":" << defaults.seed
                  << ",\"quantum\":" << defaults.quantum
                  << ",\"threads\":" << defaults.threads
                  << ",\"block_interval_ms\":" << defaults.block_interval_ms
                  << ",\"block_duration_ms\":" << defaults.block_duration_ms << "}\n";
        return 0;
    }
    // Valida todas las opciones antes de reservar estados o abrir un archivo de salida.
    const Config config = parse_arguments(argc, argv);
    // Abre trayectoria existente y pasa directamente al reproductor, sin crear un modelo físico.
    if (!config.replay_path.empty()) {
        TrajectoryReader trajectory(config.replay_path);
        replay_native(trajectory);
        return 0;
    }
    // Genera ambas galaxias antes del cronómetro para mantener la carga común entre modelos.
    auto bodies = create_galaxies(config.bodies_per_galaxy, config.seed);
    // Buffer de una aceleración por receptor; se reutiliza a través de las fases.
    std::vector<Vec2> accelerations(bodies.size());
    // Información para diagnóstico de afinidad, independiente de índices de cuerpos.
    CpuTopology topology;
    if (config.model == "smt" || config.model == "cmp") topology = detect_cpu_topology();
    // Intervalo total continuo: apertura, cálculo, captura, flush y cierre.
    // La generación aleatoria y la reproducción quedan fuera de este tiempo.
    // Inicio del intervalo comparable: incluye creación del grupo persistente y archivo.
    const auto total_start = Clock::now();
    // Propietario polimórfico; reset libera el modelo y reúne sus hilos dentro del tiempo medido.
    std::unique_ptr<ExecutionModel> model;
    // Selecciona planificación sin sustituir el kernel gravitacional ni el integrador común.
    if (config.model == "sequential") model = make_sequential_model();
    else if (config.model == "fine-grained") model = make_fine_grained_model(config.quantum);
    else if (config.model == "coarse-grained") model = make_coarse_grained_model(
        config.threads, {}, config.block_interval_ms, config.block_duration_ms);
    else if (config.model == "smt") model = make_smt_model(config.threads);
    else model = make_cmp_model(config.threads);
    // Conserva identidad, P y destinos antes de destruir el modelo.
    const std::string model_name = model->name();
    const std::size_t workers = model->workers();
    const auto assigned_cpus = model->assigned_cpus();
    // Abre y escribe cabecera; la captura forma parte del intervalo medido.
    TrajectoryWriter writer(config.trajectory_path, config, bodies.size());
    // Primera fase en posiciones iniciales para disponer de a(0) antes del primer avance.
    refresh_accelerations(bodies, accelerations, *model);
    // Captura inicial: el archivo contiene un estado más que el número de pasos.
    writer.append(bodies);
    // Los pasos se ejecutan en orden temporal, no en paralelo; cada uno depende
    // de r,v,a del anterior. El paralelismo está dentro de la fase de aceleraciones.
    for (std::size_t step = 0; step < config.steps; ++step) {
        // Integra con a(t), actualiza todas las posiciones y espera el cálculo de a(t+dt).
        advance_one_step(bodies, accelerations, config.dt, *model);
        // Guarda un estado físicamente consistente después de completar la fase.
        writer.append(bodies);
    }
    // Consulta contadores cuando los trabajadores ya no están calculando una fase.
    const SchedulingStats scheduling = model->scheduling_stats();
    // Destrucción explícita: publica parada y hace join de los trabajadores persistentes.
    model.reset();  // Incluye el cierre de los trabajadores en el tiempo total.
    // Verifica cantidad de frames, vacía buffers y cierra el archivo dentro del intervalo total.
    writer.finish();
    // Duración monotónica de pared; las siguientes impresiones y reproducción quedan fuera.
    const double total_seconds = std::chrono::duration<double>(Clock::now() - total_start).count();
    std::cerr << "Duración total (" << model_name << "): "
              << std::setprecision(6) << total_seconds << " segundos\n";
    // Una cabecera y una fila por ejecución permiten agregación externa con Make/Bash.
    // Speedup, eficiencia y estadísticas requieren comparar varias muestras/modelos.
    // Contrato CSV de una ejecución, con precisión suficiente para análisis posterior.
    std::cout << "model,workers,bodies,steps,dt,seed,quantum,total_seconds\n"
              << std::setprecision(17) << model_name << ',' << workers << ',' << bodies.size() << ',' << config.steps
              << ',' << config.dt << ',' << config.seed << ','
              << (config.model == "fine-grained" ? config.quantum : 0) << ',' << total_seconds << '\n';
    // Comprueba errores de salida antes de abrir cualquier ventana.
    std::cout.flush();
    if (!std::cout) throw std::runtime_error("No se pudieron imprimir las métricas.");
    std::cerr << "Trayectoria finalizada: " << config.trajectory_path << '\n';
    // Diagnósticos específicos: tareas lógicas, rondas y despachos no equivalen a hilos reales.
    if (config.model == "fine-grained") {
        std::cerr << "Fine-Grained simulado: 1 hilo real, " << scheduling.tasks
                  << " tareas, " << scheduling.rounds << " rondas y "
                  << scheduling.dispatches << " despachos (acumulados).\n";
    // Reporta fases y pausas acumuladas del grupo persistente.
    } else if (config.model == "coarse-grained") {
        std::cerr << "Coarse-Grained: " << workers << " hilos persistentes, "
                  << scheduling.rounds << " fases de sincronización completadas.\n"
                  << "Intervalo de bloqueo: " << config.block_interval_ms << " ms; espera: "
                  << config.block_duration_ms << " ms; bloqueos temporales: " << scheduling.blocks << ".\n";
    // Distingue hermanos SMT y sobresuscripción de software según P, C y L.
    } else if (config.model == "smt") {
        std::cerr << "SMT: " << workers << " trabajadores; CPU fijadas y verificadas: ";
        for (std::size_t i = 0; i < assigned_cpus.size(); ++i) {
            if (i != 0) std::cerr << ';';
            std::cerr << assigned_cpus[i];
        }
        std::cerr << "; ";
        print_cpu_topology(std::cerr, topology);
        if (workers > topology.logical_cpus) {
            std::cerr << "P > L: hay sobresuscripción del SO; no añade unidades SMT.\n";
        }
        if (topology.logical_cpus == topology.physical_cores) {
            std::cerr << "Sin siblings permitidos: esta ejecución no demuestra SMT de hardware.\n";
        } else if (workers <= topology.physical_cores) {
            std::cerr << "P <= C: se asigna una CPU por núcleo, sin ocupar siblings del mismo núcleo.\n";
        } else {
            std::cerr << "Se asignan CPUs hermanas del mismo núcleo; con varios núcleos se combina CMP + SMT.\n";
        }
    // Registra destinos físicos o administración por SO según plataforma.
    } else if (config.model == "cmp") {
        std::cerr << "CMP: " << workers << " trabajadores; ";
#if defined(__linux__)
        std::cerr << "CPU fijadas por núcleo físico: ";
        for (std::size_t i = 0; i < workers; ++i) {
            if (i != 0) std::cerr << ';';
            std::cerr << topology.core_cpus[i];
        }
        std::cerr << "; ";
#else
        std::cerr << "afinidad administrada por el SO; ";
#endif
        print_cpu_topology(std::cerr, topology);
    }
    std::cerr.flush();

    // La ventana solo se crea después de cerrar el archivo e imprimir los resultados.
    // Solo después de imprimir y cerrar captura se abre la reproducción SDL.
    if (!config.no_output) {
        TrajectoryReader trajectory(config.trajectory_path);
        replay_native(trajectory);
    }
    return 0;
// Los recursos RAII liberan archivos y modelos al propagar el error hacia este punto.
} catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << "\nUse --help para ver las opciones.\n";
    return 1;
}
