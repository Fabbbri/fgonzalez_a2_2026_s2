/**
 * @file config.cpp
 * Analizador de argumentos de la implementación secuencial anterior.
 * Utiliza include/nbody/config.hpp; no pertenece a SOURCES del Makefile actual.
 * La entrada activa define su configuración multihilo en src/main.cpp y src/nbody.hpp.
 */
#include "nbody/config.hpp"
#include <charconv>
#include <cmath>
#include <limits>
#include <ostream>
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

// Validamos los parámetros antes de reservar memoria o producir el CSV.
// Construye configuración validada a partir de argv; argv[0] es el nombre del programa.
Config parse_arguments(int argc, char* argv[]) {
    // Parte de los valores predeterminados de la cabecera correspondiente a esta ruta.
    Config config;
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
            option != "--dt" && option != "--seed") {
            throw std::invalid_argument("Opción desconocida: " + option);
        }
        // Consume argumento de valor y detecta una opción final sin valor.
        if (++i == argc) {
            throw std::invalid_argument("Falta el valor de " + option);
        }
        const std::string value = argv[i];
        if (option == "--bodies-per-galaxy") {
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
              "  --bodies-per-galaxy N  Cuerpos por galaxia (100)\n"
              "  --steps N              Pasos; 0 = estado inicial (10000)\n"
              "  --dt X                 Duración de cada paso (0.002)\n"
              "  --seed N               Semilla de 32 bits (42)\n"
              "  --no-output            Ejecutar sin ventana para medir rendimiento\n"
              "Por defecto abre una ventana nativa 2D. No genera CSV.\n"
              "Espacio: pausa; N: un paso; R: reiniciar; F: encuadrar.\n"
              "Rueda: zoom; arrastrar: mover cámara; +/-: velocidad; Esc: salir.\n"
              "Tiempos y diagnóstico inicial/final por stderr, fuera de la visualización.\n";
}
}  // namespace nbody
