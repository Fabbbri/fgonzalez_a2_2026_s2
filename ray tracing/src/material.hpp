#pragma once

namespace rt {
// El color de la esfera actúa como reflectancia del material.
enum class MaterialType { diffuse, metal, glass };
struct Material {
    MaterialType type = MaterialType::diffuse;
    double roughness = 0.0; // Metal: 0 es un espejo, 1 dispersa mucho el reflejo.
    double refractive_index = 1.5; // Vidrio en aire.
};
} // namespace rt
