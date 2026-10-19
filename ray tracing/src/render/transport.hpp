/**
 * @file transport.hpp
 * Interfaz del transporte de luz: reflexión, refracción y trayectoria.
 * Los vectores de dirección y normales usados en las fórmulas ópticas
 * son unitarios. El generador recibido pertenece al píxel actual.
 */
#pragma once

#include "scene/scene.hpp"
#include "scene/sampling.hpp"

namespace rt {
// Reflexión especular respecto a una normal unitaria.
Vec3 reflect(Vec3 direction, Vec3 normal);
// normal se opone a direction; eta = índice de entrada / índice de salida.
// Retorna false si hay reflexión interna total.
// Reconstruye la dirección transmitida por Snell y detecta reflexión interna total.
bool refract(Vec3 direction, Vec3 normal, double eta, Vec3& transmitted);
// Radiancia RGB estimada del rayo tras interacciones sucesivas y escape al cielo.
Vec3 trace_path(const Scene& scene, Ray ray, Sampler& sampler);
} // namespace rt
