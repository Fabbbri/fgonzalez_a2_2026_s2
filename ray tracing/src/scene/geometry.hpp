/**
 * @file geometry.hpp
 * Tipos geométricos y operaciones vectoriales de tres componentes.
 * Vec3 representa posición, dirección o color según el contexto; la
 * multiplicación componente a componente modela la atenuación RGB.
 */
#pragma once

#include <cmath>
#include "scene/material.hpp"

namespace rt {
// Vector cartesiano o RGB. Cada operación devuelve un valor nuevo.
struct Vec3 {
    // Componentes escalares; en color corresponden a rojo, verde y azul.
    double x, y, z;
    // Suma componente a componente, usada para posiciones y acumulación de luz.
    Vec3 operator+(Vec3 b) const { return {x + b.x, y + b.y, z + b.z}; }
    // Diferencia vectorial, usada para desplazamientos y normales.
    Vec3 operator-(Vec3 b) const { return {x - b.x, y - b.y, z - b.z}; }
    // Escalamiento uniforme por un escalar.
    Vec3 operator*(double k) const { return {x * k, y * k, z * k}; }
    // Producto de Hadamard: atenuación independiente de cada canal RGB.
    Vec3 operator*(Vec3 b) const { return {x * b.x, y * b.y, z * b.z}; }
};
// Producto escalar; permite medir proyecciones y normas al cuadrado.
inline double dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
// Devuelve longitud uno; el llamante garantiza norma no nula.
inline Vec3 normalized(Vec3 v) { return v * (1.0 / std::sqrt(dot(v, v))); }
// Rayo paramétrico R(t)=origin+t*direction; dirección unitaria en el trazador.
struct Ray { Vec3 origin, direction; };
// Geometría esférica con reflectancia RGB y parámetros ópticos por valor.
struct Sphere { Vec3 center; double radius; Vec3 color; Material material{}; };
// true entrega en distance la primera raíz positiva; false deja salida sin contrato.
bool intersect(const Ray& ray, const Sphere& sphere, double& distance);
} // namespace rt
