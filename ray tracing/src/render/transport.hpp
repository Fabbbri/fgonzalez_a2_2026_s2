#pragma once

#include "scene/scene.hpp"
#include "scene/sampling.hpp"

namespace rt {
Vec3 reflect(Vec3 direction, Vec3 normal);
// normal se opone a direction; eta = índice de entrada / índice de salida.
// Retorna false si hay reflexión interna total.
bool refract(Vec3 direction, Vec3 normal, double eta, Vec3& transmitted);
Vec3 trace_path(const Scene& scene, Ray ray, Sampler& sampler);
} // namespace rt
