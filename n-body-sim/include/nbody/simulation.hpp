/**
 * @file simulation.hpp
 * Tipos y física de la interfaz secuencial anterior de simulación.
 * Sus funciones no reciben ExecutionModel. El contrato multihilo activo está
 * en src/nbody.hpp y su cálculo compartido en src/common/physics.cpp.
 */
#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace nbody {
// Posiciones, velocidades y aceleraciones viven en el plano XY.
// Vector 2D con componentes inicializadas a cero.
struct Vec2 {
    double x = 0.0;
    double y = 0.0;
};

// Estado de partícula: masa, coordenadas, velocidad, aceleración y población de origen.
struct Body {
    // Masa positiva de la partícula, constante durante la simulación.
    double mass;
    // Coordenadas cartesianas de la instantánea física.
    Vec2 position;
    // Velocidad empleada por el integrador.
    Vec2 velocity;
    // Última aceleración calculada a partir de las posiciones actuales.
    Vec2 acceleration;
    // Identificador de origen para visualización, sin filtrar atracción gravitacional.
    int galaxy_id;  // Conserva la galaxia de origen de cada cuerpo.
};


// Unidades normalizadas y suavizado compartido con el cálculo de energía.
// G normalizada y epsilon comparten las mismas unidades que posiciones y masas.
inline constexpr double gravitational_constant = 1.0;
inline constexpr double softening = 0.05;

// Genera condiciones iniciales de dos discos con la misma semilla.
std::vector<Body> create_galaxies(std::size_t bodies_per_galaxy, std::uint32_t seed);
// Kernel directo secuencial i,j; entrada inmutable y salida separada.
void compute_accelerations(const std::vector<Body>& bodies,
                           std::vector<Vec2>& accelerations,
                           double gravitational_constant, double softening);
// Valida el resultado y lo incorpora a las partículas.
void refresh_accelerations(std::vector<Body>& bodies, std::vector<Vec2>& accelerations);
// Requiere aceleraciones correspondientes a las posiciones actuales.
// Actualiza velocidades y posiciones y prepara a(t+dt) para el siguiente paso.
void advance_one_step(std::vector<Body>& bodies, std::vector<Vec2>& accelerations, double dt);
}  // namespace nbody
