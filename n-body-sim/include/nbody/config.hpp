/**
 * @file config.hpp
 * Configuración de la interfaz secuencial anterior, usada por src/config.cpp.
 * Sus valores predeterminados son propios de esa ruta y difieren del Config
 * activo de src/nbody.hpp; no se combinan ambas definiciones en una misma unidad.
 */
#pragma once
#include <cstddef>
#include <cstdint>
#include <iosfwd>

namespace nbody {
// Parámetros de la ruta anterior; no contiene estrategia de ejecución.
struct Config {
    // Población por disco; total N igual al doble.
    std::size_t bodies_per_galaxy = 100;
    // Cantidad de pasos físicos, no frames por segundo.
    std::size_t steps = 10000;
    // Paso temporal positivo de Euler simpléctico.
    double dt = 0.002;
    // Semilla determinista de inicialización.
    std::uint32_t seed = 42;
    // En esta interfaz anterior, selecciona ejecución sin ventana.
    bool no_output = false;  // Ejecutar sin ventana para medir rendimiento.
};

// Analiza la configuración de la ruta anterior de src/config.cpp.
Config parse_arguments(int argc, char* argv[]);
// Escribe los parámetros y controles propios de esa interfaz.
void print_usage(std::ostream& output);
}  // namespace nbody
