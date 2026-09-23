#include "simulation.hpp"
#include <cmath>
#include <random>
#include <stdexcept>
#include <string>

namespace nbody {
namespace {
constexpr double galaxy_radius = 1.0;
constexpr double galaxy_mass = 1.0;
constexpr double angular_speed = 0.5;
constexpr double pi = 3.14159265358979323846;

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

}  // namespace

std::vector<Body> create_galaxies(std::size_t bodies_per_galaxy, std::uint32_t seed) {
    std::vector<Body> bodies;
    if (bodies_per_galaxy == 0 || bodies_per_galaxy > bodies.max_size() / 2) {
        throw std::invalid_argument("Cantidad de cuerpos no válida.");
    }
    bodies.reserve(2 * bodies_per_galaxy);
    std::mt19937 generator(seed);
    add_galaxy(bodies, {-3.0, 0.0}, {0.2, 0.0}, 0, bodies_per_galaxy, generator);
    add_galaxy(bodies, {3.0, 0.0}, {-0.2, 0.0}, 1, bodies_per_galaxy, generator);
    return bodies;
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

}  // namespace nbody
