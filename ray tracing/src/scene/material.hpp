/**
 * @file material.hpp
 * Parámetros ópticos de los materiales: tipo, rugosidad e índice de refracción.
 * La reflectancia RGB se almacena en Sphere.color; el vidrio no la usa como tinte.
 */
#pragma once

namespace rt {
// El color de la esfera actúa como reflectancia del material.
// Enumeración de las tres leyes de interacción implementadas por trace_path.
enum class MaterialType { diffuse, metal, glass };
struct Material {
    // Difuso por defecto; el tipo selecciona la rama de transporte de luz.
    MaterialType type = MaterialType::diffuse;
    // Amplitud de la perturbación metálica, interpretada habitualmente en [0,1].
    double roughness = 0.0; // Metal: 0 es un espejo, 1 dispersa mucho el reflejo.
    // Índice óptico absoluto del vidrio; el medio exterior se considera aire n=1.
    double refractive_index = 1.5; // Vidrio en aire.
};
} // namespace rt
