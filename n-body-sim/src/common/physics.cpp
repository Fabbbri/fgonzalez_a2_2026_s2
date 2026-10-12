#include "nbody.hpp"
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

// La misma ecuación y orden de suma pueden usarse en cada estrategia por cuerpo.
Vec2 acceleration_for_body(const std::vector<Body>& bodies, std::size_t index,
                           double gravity, double epsilon) {
    Vec2 acceleration{};
    accumulate_accelerations(bodies, index, 0, bodies.size(), acceleration, gravity, epsilon);
    return acceleration;
}

void accumulate_accelerations(const std::vector<Body>& bodies, std::size_t index,
                              std::size_t begin, std::size_t end, Vec2& acceleration,
                              double gravity, double epsilon) {
    const double epsilon_squared = epsilon * epsilon;
    for (std::size_t j = begin; j < end; ++j) {
        if (index == j) continue;
        const double dx = bodies[j].position.x - bodies[index].position.x;
        const double dy = bodies[j].position.y - bodies[index].position.y;
        const double distance_squared = dx * dx + dy * dy + epsilon_squared;
        const double factor = gravity * bodies[j].mass /
                              (distance_squared * std::sqrt(distance_squared));
        acceleration.x += factor * dx;
        acceleration.y += factor * dy;
    }
}

bool is_finite(Vec2 value) {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

void refresh_accelerations(std::vector<Body>& bodies, std::vector<Vec2>& accelerations,
                           ExecutionModel& model) {
    model.compute_accelerations(bodies, accelerations, gravitational_constant, softening);
    if (accelerations.size() != bodies.size()) {
        throw std::runtime_error("El modelo devolvió una cantidad incorrecta de aceleraciones.");
    }
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
                      double dt, ExecutionModel& model) {
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
    refresh_accelerations(bodies, accelerations, model);
}

}  // namespace nbody
