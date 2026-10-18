/**
 * @file physics.cpp
 * Física común a las cinco estrategias del ejecutable multihilo.
 * Cada fase evalúa a_i = suma(j!=i) G*m_j*(r_j-r_i)/(distancia²+epsilon²)^(3/2).
 * Las posiciones fuente se mantienen fijas durante el cálculo de la fase y
 * las aceleraciones se escriben por separado. El costo es O(N²) por fase.
 * El integrador Euler simpléctico actualiza primero velocidades y luego posiciones.
 * Tras actualizar todos los cuerpos, calcula las aceleraciones del nuevo estado.
 */
#include "nbody.hpp"
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

// La misma ecuación y orden de suma pueden usarse en cada estrategia por cuerpo.
// Suma todas las fuentes j para un receptor index=i con un acumulador privado.
Vec2 acceleration_for_body(const std::vector<Body>& bodies, std::size_t index,
                           double gravity, double epsilon) {
    // Acumulador cero de dos componentes; nunca compartido entre trabajadores.
    Vec2 acceleration{};
    // Intervalo completo [0,N) de fuentes, con orden idéntico en cada estrategia.
    accumulate_accelerations(bodies, index, 0, bodies.size(), acceleration, gravity, epsilon);
    return acceleration;
}

// Kernel reanudable sobre fuentes [begin,end) para un receptor index fijo.
// La salida acceleration puede conservar aportes previos de la misma tarea fine.
void accumulate_accelerations(const std::vector<Body>& bodies, std::size_t index,
                              std::size_t begin, std::size_t end, Vec2& acceleration,
                              double gravity, double epsilon) {
    // Cuadrado del suavizado: regulariza encuentros cercanos y posiciones coincidentes.
    const double epsilon_squared = epsilon * epsilon;
    // j es el cuerpo fuente; este bucle no se reparte entre hilos.
    // Cada receptor acumula fuentes de ambas galaxias en orden ascendente.
    for (std::size_t j = begin; j < end; ++j) {
        // Omite autoatracción; no aporta fuerza ni consume una interacción física de fine.
        if (index == j) continue;
        // Desplazamiento horizontal r_j-r_i hacia la fuente; dy es su componente vertical.
        const double dx = bodies[j].position.x - bodies[index].position.x;
        const double dy = bodies[j].position.y - bodies[index].position.y;
        // Distancia euclidiana al cuadrado más epsilon²; siempre positiva para epsilon>0.
        const double distance_squared = dx * dx + dy * dy + epsilon_squared;
        // G*m_j/(r²+epsilon²)^(3/2). La masa del receptor no aparece al calcular aceleración.
        const double factor = gravity * bodies[j].mass /
                              (distance_squared * std::sqrt(distance_squared));
        // Acumulación ordenada del aporte horizontal; la siguiente línea suma el vertical.
        acceleration.x += factor * dx;
        acceleration.y += factor * dy;
    }
}

// Comprueba las dos componentes para detectar NaN e infinitos antes de propagar estado.
bool is_finite(Vec2 value) {
    return std::isfinite(value.x) && std::isfinite(value.y);
}

// Calcula una fase completa de fuerzas y transfiere resultados validados a los cuerpos.
void refresh_accelerations(std::vector<Body>& bodies, std::vector<Vec2>& accelerations,
                           ExecutionModel& model) {
    // Barrera contractual: al retornar no quedan trabajadores leyendo bodies.
    // Solo entonces se valida y copia el vector accelerations al estado físico.
    model.compute_accelerations(bodies, accelerations, gravitational_constant, softening);
    // Comprueba que la estrategia entregó exactamente una aceleración por cuerpo.
    if (accelerations.size() != bodies.size()) {
        throw std::runtime_error("El modelo devolvió una cantidad incorrecta de aceleraciones.");
    }
    // Recorrido del coordinador: la fase de fuerzas ya terminó antes de copiar a Body.
    // En la ruta secuencial anterior, este mismo patrón también aparece en el kernel.
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
                      double dt, ExecutionModel& model) {
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
    refresh_accelerations(bodies, accelerations, model);
}

}  // namespace nbody
