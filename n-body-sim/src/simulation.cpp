/**
 * @file simulation.cpp
 * Implementación secuencial anterior de física e integración en dos dimensiones.
 * Se relaciona con include/nbody/simulation.hpp y no se enlaza en build/nbody
 * según el Makefile actual. common/physics.cpp contiene la ruta con ExecutionModel.
 * Aquí se preserva el cálculo O(N²) mediante bucles i,j y Euler simpléctico.
 */
#include "nbody/simulation.hpp"
#include <cmath>
#include <random>
#include <stdexcept>
#include <string>

namespace nbody {
namespace {
// Radio inicial del disco en unidades normalizadas.
constexpr double galaxy_radius = 1.0;
// Masa total de una galaxia; se divide uniformemente entre sus cuerpos.
constexpr double galaxy_mass = 1.0;
// Velocidad angular prescrita para la condición inicial, no equilibrio orbital exacto.
constexpr double angular_speed = 0.5;
// Constante de conversión del ángulo uniforme al intervalo [0,2*pi).
constexpr double pi = 3.14159265358979323846;

// Ambas galaxias usan el mismo método de distribución y rotación.
// Agrega cuerpos de un disco con velocidad de rotación y traslación del centro.
// Este bucle construye la entrada antes del intervalo de simulación medido.
void add_galaxy(std::vector<Body>& bodies, Vec2 center, Vec2 bulk_velocity,
                int galaxy_id, std::size_t bodies_per_galaxy, std::mt19937& generator) {
    // Genera U en [0,1) a partir del mismo estado pseudoaleatorio recibido.
    std::uniform_real_distribution<double> uniform(0.0, 1.0);

    // i aquí enumera nuevas partículas de una galaxia; aún no es el reparto multihilo.
    for (std::size_t i = 0; i < bodies_per_galaxy; ++i) {
        // sqrt(U) distribuye los cuerpos uniformemente en el área del disco.
        // La transformación sqrt(U) compensa el crecimiento de área del anillo con el radio.
        const double radius = galaxy_radius * std::sqrt(uniform(generator));
        // Azimut uniforme para una distribución circular sin dirección preferida.
        const double angle = 2.0 * pi * uniform(generator);
        // Transformación polar-cartesiana relativa al centro de la galaxia.
        const Vec2 offset{radius * std::cos(angle), radius * std::sin(angle)};

        // Rotación inicial sencilla: v = omega * (-y, x), más traslación.
        // La rotación prescrita es una condición didáctica sin equilibrio gravitacional exacto.
        // Suma velocidad del centro y rotación omega*(-y,x) del desplazamiento local.
        const Vec2 velocity{bulk_velocity.x - angular_speed * offset.y,
                            bulk_velocity.y + angular_speed * offset.x};

        // Inserta masa, posición global, velocidad y aceleración cero; galaxy_id
        // identifica origen para presentación y no restringe interacciones gravitacionales.
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

// Crea un único vector de N=2*bodies_per_galaxy partículas de ambas poblaciones.
std::vector<Body> create_galaxies(std::size_t bodies_per_galaxy, std::uint32_t seed) {
    std::vector<Body> bodies;
    // Impide entrada vacía y desbordamiento al duplicar la cantidad de cuerpos.
    if (bodies_per_galaxy == 0 || bodies_per_galaxy > bodies.max_size() / 2) {
        throw std::invalid_argument("Cantidad de cuerpos no válida.");
    }
    // Reserva una sola vez para evitar realocaciones durante la distribución inicial.
    bodies.reserve(2 * bodies_per_galaxy);
    // Semilla fija compartida por la generación consecutiva de ambas galaxias.
    std::mt19937 generator(seed);
    // Primera población, desplazada hacia la izquierda y con velocidad hacia la derecha.
    add_galaxy(bodies, {-3.0, 0.0}, {0.2, 0.0}, 0, bodies_per_galaxy, generator);
    // Segunda población simétrica, con velocidad hacia la izquierda.
    add_galaxy(bodies, {3.0, 0.0}, {-0.2, 0.0}, 1, bodies_per_galaxy, generator);
    return bodies;
}

// Cada cuerpo recibe la atracción de todos los demás, de ambas galaxias.
// Costo O(N²): leemos posiciones fijas y guardamos el resultado por separado.
// Kernel anterior directo: bucle exterior de receptores i e interior de fuentes j.
void compute_accelerations(const std::vector<Body>& bodies,
                           std::vector<Vec2>& accelerations,
                           double gravitational_constant, double softening) {
    // Reserva y pone a cero todas las salidas para acumular la nueva instantánea.
    accelerations.assign(bodies.size(), Vec2{});
    // Suavizado al cuadrado común al potencial de diagnóstico.
    const double softening_squared = softening * softening;

    // Bucle exterior de receptores i; cada salida acumula en orden las fuentes j.
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        // Cada receptor i consulta los N cuerpos; la suma excluye j==i.
        for (std::size_t j = 0; j < bodies.size(); ++j) {
            if (i == j) {
                continue;  // Un cuerpo no ejerce atracción sobre sí mismo.
            }

            // Desplazamiento de la fuente j respecto al receptor i.
            const double dx = bodies[j].position.x - bodies[i].position.x;
            const double dy = bodies[j].position.y - bodies[i].position.y;
            // Norma al cuadrado regularizada: dx²+dy²+epsilon².
            const double distance_squared = dx * dx + dy * dy + softening_squared;

            // a_i += G * m_j * (r_j - r_i) / (|r_j - r_i|² + epsilon²)^(3/2).
            // El suavizado evita dividir entre cero si dos cuerpos coinciden.
            const double factor = gravitational_constant * bodies[j].mass /
                                  (distance_squared * std::sqrt(distance_squared));
            // Suma el aporte horizontal; la componente vertical se acumula por separado.
            accelerations[i].x += factor * dx;
            accelerations[i].y += factor * dy;
        }
    }
}

// Comprueba las dos componentes para detectar NaN e infinitos antes de propagar estado.
bool is_finite(Vec2 value) {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

// Calcula una fase completa de fuerzas y transfiere resultados validados a los cuerpos.
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
// Integrador Euler simpléctico: usa a(t), actualiza v y r, y prepara a(t+dt).
// Los pasos temporales son secuenciales porque cada uno depende del estado anterior.
void advance_one_step(std::vector<Body>& bodies, std::vector<Vec2>& accelerations,
                      double dt) {
    // Actualización de cuerpos en el coordinador, fuera de una fase concurrente de fuerzas.
    for (Body& body : bodies) {
        // v_x(t+dt)=v_x(t)+a_x(t)*dt; análogamente para y.
        body.velocity.x += body.acceleration.x * dt;
        body.velocity.y += body.acceleration.y * dt;
        // r_x(t+dt)=r_x(t)+v_x(t+dt)*dt: emplea la velocidad recién actualizada.
        body.position.x += body.velocity.x * dt;
        body.position.y += body.velocity.y * dt;
        // Un paso que genera estado no finito se rechaza antes de otra fase o captura.
        if (!is_finite(body.position) || !is_finite(body.velocity)) {
            throw std::runtime_error("Estado no finito: revise dt y las condiciones iniciales.");
        }
    }

    // Todas las posiciones ya están en t+dt. Preparamos a(t+dt) para la salida
    // y para el siguiente paso, sin mezclar estados temporales en las fuerzas.
    refresh_accelerations(bodies, accelerations);
}

}  // namespace nbody
