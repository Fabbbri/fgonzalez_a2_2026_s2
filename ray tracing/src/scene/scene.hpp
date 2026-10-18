/**
 * @file scene.hpp
 * Datos compartidos de solo lectura durante el renderizado.
 * Agrupa geometría, iluminación del cielo y calidad de integración por píxel.
 */
#pragma once

#include "scene/geometry.hpp"
#include <vector>
#include <cstdint>

namespace rt {
// Parámetros comunes que fijan la carga óptica y la reproducibilidad.
struct RenderSettings {
    // Número de muestras por píxel; cada muestra sigue una trayectoria.
    int samples = 128;
    // Máximo de interacciones antes de descartar una trayectoria sin escape.
    int max_bounces = 12;
    // Semilla combinada con las coordenadas de cada píxel.
    std::uint64_t seed = 2026;
};
// Conjunto geométrico e iluminación consultados sin modificación por trabajadores.
struct Scene {
    // Geometría; la búsqueda de impacto recorre estas esferas en cada segmento.
    std::vector<Sphere> spheres;
    // El cielo ilumina la escena: las superficies reciben luz de sus rebotes.
    // Color del cielo superior empleado en la interpolación del entorno.
    Vec3 background{0.50, 0.70, 1.0};
    // Color próximo al horizonte; aporta el extremo inferior de la mezcla.
    Vec3 horizon{1.0, 1.0, 1.0};
    // Calidad y semilla que comparte toda la imagen.
    RenderSettings settings;
};
// Construye suelo, protagonistas y decoraciones con una secuencia determinista.
Scene default_scene();
} // namespace rt
