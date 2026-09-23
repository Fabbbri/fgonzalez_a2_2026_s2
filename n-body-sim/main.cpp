#include <cmath>
#include <cstddef>
#include <iomanip>
#include <iostream>
#include <random>
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
// Parámetros iniciales en unidades normalizadas, sin unidades astronómicas.
constexpr std::size_t bodies_per_galaxy = 100;
constexpr double galaxy_radius = 1.0;
constexpr double galaxy_mass = 1.0;
constexpr double angular_speed = 0.5;
constexpr double pi = 3.14159265358979323846;

// Ambas galaxias usan el mismo método de distribución y rotación.
void add_galaxy(std::vector<Body>& bodies, Vec2 center, Vec2 bulk_velocity,
                int galaxy_id, std::mt19937& generator) {
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
            {},  // Las aceleraciones se calcularán en el siguiente paso.
            galaxy_id,
        });
    }
}

// El CSV permite inspeccionar el estado inicial sin añadir un visualizador.
void write_csv(const std::vector<Body>& bodies, std::ostream& output) {
    output << "id,galaxy,mass,x,y,vx,vy,ax,ay\n";
    output << std::setprecision(17);

    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const Body& body = bodies[i];
        output << i << ',' << body.galaxy_id << ',' << body.mass << ','
               << body.position.x << ',' << body.position.y << ','
               << body.velocity.x << ',' << body.velocity.y << ','
               << body.acceleration.x << ',' << body.acceleration.y << '\n';
    }
}
}  // namespace

int main() {
    // La semilla fija permite repetir la inicialización con la misma biblioteca.
    std::mt19937 generator(42);
    std::vector<Body> bodies;
    bodies.reserve(2 * bodies_per_galaxy);

    // Centros separados; las velocidades de traslación apuntan al encuentro.
    add_galaxy(bodies, {-3.0, 0.0}, {0.2, 0.0}, 0, generator);
    add_galaxy(bodies, {3.0, 0.0}, {-0.2, 0.0}, 1, generator);

    write_csv(bodies, std::cout);
    return std::cout ? 0 : 1;
}
