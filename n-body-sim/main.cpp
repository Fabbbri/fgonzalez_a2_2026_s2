#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

// Posiciones, velocidades y aceleraciones viven en el plano XY.
struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

struct Body {
    double mass;
    Vec2 position;
    Vec2 velocity;
    Vec2 acceleration;
    int galaxy_id;  // Conserva la galaxia de origen de cada cuerpo.
};

namespace {
struct Config {
    std::size_t bodies_per_galaxy = 100;
    std::size_t steps = 10000;
    double dt = 0.002;
    std::size_t output_every = 100;
    std::uint32_t seed = 42;
};

// Parámetros iniciales en unidades normalizadas, sin unidades astronómicas.
constexpr double galaxy_radius = 1.0;
constexpr double galaxy_mass = 1.0;
constexpr double angular_speed = 0.5;
constexpr double gravitational_constant = 1.0;
constexpr double softening = 0.05;  // Longitud de suavizado; debe ser positiva.
constexpr double pi = 3.14159265358979323846;

std::size_t parse_integer(const std::string& value) {
    std::size_t result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) {
        throw std::invalid_argument("Entero no válido: " + value);
    }
    return result;
}

// Validamos los parámetros antes de reservar memoria o producir el CSV.
Config parse_arguments(int argc, char* argv[]) {
    Config config;
    for (int i = 1; i < argc; ++i) {
        const std::string option = argv[i];
        if (option != "--bodies-per-galaxy" && option != "--steps" &&
            option != "--dt" && option != "--output-every" && option != "--seed") {
            throw std::invalid_argument("Opción desconocida: " + option);
        }
        if (++i == argc) {
            throw std::invalid_argument("Falta el valor de " + option);
        }
        const std::string value = argv[i];
        if (option == "--bodies-per-galaxy") {
            config.bodies_per_galaxy = parse_integer(value);
        } else if (option == "--steps") {
            config.steps = parse_integer(value);
        } else if (option == "--output-every") {
            config.output_every = parse_integer(value);
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
    if (config.bodies_per_galaxy == 0 || config.output_every == 0) {
        throw std::invalid_argument("Los cuerpos y la frecuencia de salida deben ser positivos.");
    }
    if (!std::isfinite(config.dt) || config.dt <= 0.0 ||
        !std::isfinite(config.dt * static_cast<double>(config.steps))) {
        throw std::invalid_argument("dt debe ser positivo y el tiempo final debe ser finito.");
    }
    return config;
}

// Ambas galaxias usan el mismo método de distribución y rotación.
void add_galaxy(std::vector<Body>& bodies, Vec2 center, Vec2 bulk_velocity,
                int galaxy_id, std::size_t bodies_per_galaxy, std::mt19937& generator) {
    std::uniform_real_distribution<double> uniform(0.0, 1.0);

    for (std::size_t i = 0; i < bodies_per_galaxy; ++i) {
        // sqrt(U) distribuye los cuerpos uniformemente en el área del disco.
        const double radius = galaxy_radius * std::sqrt(uniform(generator));
        const double angle = 2.0 * pi * uniform(generator);
        const Vec2 offset{radius * std::cos(angle), radius * std::sin(angle)};

        // Rotación inicial sencilla: v = omega * (-y, x), más traslación.
        // Esta elección no representa todavía un equilibrio gravitacional.
        const Vec2 velocity{bulk_velocity.x - angular_speed * offset.y,
                            bulk_velocity.y + angular_speed * offset.x};

        bodies.push_back(Body{
            galaxy_mass / static_cast<double>(bodies_per_galaxy),
            {center.x + offset.x, center.y + offset.y},
            velocity,
            {},  // Se calcula después de inicializar ambas galaxias.
            galaxy_id,
        });
    }
}

// Cada cuerpo recibe la atracción de todos los demás, de ambas galaxias.
// Costo O(N²): leemos posiciones fijas y guardamos el resultado por separado.
void compute_accelerations(const std::vector<Body>& bodies,
                           std::vector<Vec2>& accelerations,
                           double gravitational_constant, double softening) {
    accelerations.assign(bodies.size(), Vec2{});
    const double softening_squared = softening * softening;

    for (std::size_t i = 0; i < bodies.size(); ++i) {
        for (std::size_t j = 0; j < bodies.size(); ++j) {
            if (i == j) {
                continue;  // Un cuerpo no ejerce atracción sobre sí mismo.
            }

            const double dx = bodies[j].position.x - bodies[i].position.x;
            const double dy = bodies[j].position.y - bodies[i].position.y;
            const double distance_squared = dx * dx + dy * dy + softening_squared;

            // a_i += G * m_j * (r_j - r_i) / (|r_j - r_i|² + epsilon²)^(3/2).
            // El suavizado evita dividir entre cero si dos cuerpos coinciden.
            const double factor = gravitational_constant * bodies[j].mass /
                                  (distance_squared * std::sqrt(distance_squared));
            accelerations[i].x += factor * dx;
            accelerations[i].y += factor * dy;
        }
    }
}

bool is_finite(Vec2 value) {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

void refresh_accelerations(std::vector<Body>& bodies, std::vector<Vec2>& accelerations) {
    compute_accelerations(bodies, accelerations, gravitational_constant, softening);
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        if (!is_finite(accelerations[i])) {
            throw std::runtime_error("Aceleración no finita en el cuerpo " + std::to_string(i));
        }
        bodies[i].acceleration = accelerations[i];
    }
}

// Euler simpléctico: primero v(t+dt) = v(t) + a(t)*dt;
// después r(t+dt) = r(t) + v(t+dt)*dt, usando la velocidad nueva.
void advance_one_step(std::vector<Body>& bodies, std::vector<Vec2>& accelerations,
                      double dt) {
    for (Body& body : bodies) {
        body.velocity.x += body.acceleration.x * dt;
        body.velocity.y += body.acceleration.y * dt;
        body.position.x += body.velocity.x * dt;
        body.position.y += body.velocity.y * dt;
        if (!is_finite(body.position) || !is_finite(body.velocity)) {
            throw std::runtime_error("Estado no finito: revise dt y las condiciones iniciales.");
        }
    }

    // Todas las posiciones ya están en t+dt. Preparamos a(t+dt) para la salida
    // y para el siguiente paso, sin mezclar estados temporales en las fuerzas.
    refresh_accelerations(bodies, accelerations);
}

// Una fila por cuerpo y momento; el encabezado se escribe una sola vez.
void write_snapshot(const std::vector<Body>& bodies, std::size_t step, double time,
                    std::ostream& output) {
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const Body& body = bodies[i];
        output << step << ',' << time << ',' << i << ',' << body.galaxy_id << ',' << body.mass << ','
               << body.position.x << ',' << body.position.y << ','
               << body.velocity.x << ',' << body.velocity.y << ','
               << body.acceleration.x << ',' << body.acceleration.y << '\n';
    }
    if (!output) {
        throw std::runtime_error("No se pudo escribir la salida CSV.");
    }
}
}  // namespace

int main(int argc, char* argv[]) try {
    if (argc == 2 && std::string(argv[1]) == "--help") {
        std::cout << "Uso: nbody [opciones]\n"
                     "  --bodies-per-galaxy N  Cuerpos por galaxia (100)\n"
                     "  --steps N              Pasos temporales; 0 = estado inicial (10000)\n"
                     "  --dt X                 Duración de cada paso (0.002)\n"
                     "  --output-every N       Guardar cada N pasos, más inicial y final (100)\n"
                     "  --seed N               Semilla de 32 bits (42)\n"
                     "CSV por stdout; parámetros y tiempo de cálculo por stderr.\n";
        return 0;
    }
    const Config config = parse_arguments(argc, argv);
    // La semilla fija permite repetir la inicialización con la misma biblioteca.
    std::mt19937 generator(config.seed);
    std::vector<Body> bodies;
    if (config.bodies_per_galaxy > bodies.max_size() / 2) {
        throw std::invalid_argument("Demasiados cuerpos para el tamaño del vector.");
    }
    bodies.reserve(2 * config.bodies_per_galaxy);

    // Centros separados; las velocidades de traslación apuntan al encuentro.
    add_galaxy(bodies, {-3.0, 0.0}, {0.2, 0.0}, 0, config.bodies_per_galaxy, generator);
    add_galaxy(bodies, {3.0, 0.0}, {-0.2, 0.0}, 1, config.bodies_per_galaxy, generator);

    std::vector<Vec2> accelerations;
    using Clock = std::chrono::steady_clock;
    const auto initial_start = Clock::now();
    refresh_accelerations(bodies, accelerations);
    auto compute_time = Clock::now() - initial_start;

    std::cout << "step,time,id,galaxy,mass,x,y,vx,vy,ax,ay\n" << std::setprecision(17);
    write_snapshot(bodies, 0, 0.0, std::cout);

    for (std::size_t index = 0; index < config.steps; ++index) {
        const auto start = Clock::now();
        advance_one_step(bodies, accelerations, config.dt);
        compute_time += Clock::now() - start;

        // La escritura queda fuera de los intervalos cronometrados.
        const std::size_t step = index + 1;
        if (step % config.output_every == 0 || step == config.steps) {
            write_snapshot(bodies, step, static_cast<double>(step) * config.dt, std::cout);
        }
    }

    std::cout.flush();
    if (!std::cout) {
        throw std::runtime_error("No se pudo completar la salida CSV.");
    }
    std::cerr << std::setprecision(10)
              << "Cuerpos: " << bodies.size() << ", pasos: " << config.steps
              << ", dt: " << config.dt << ", semilla: " << config.seed
              << ", salida cada: " << config.output_every << " pasos\n"
              << "Tiempo simulado: " << static_cast<double>(config.steps) * config.dt << '\n'
              << "Tiempo de cálculo secuencial (sin inicialización aleatoria ni E/S): "
              << std::chrono::duration<double>(compute_time).count() << " s\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << "Error: " << error.what() << "\nUse --help para ver las opciones.\n";
    return 1;
}
