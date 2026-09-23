#pragma once

#include <cmath>
#include "material.hpp"

namespace rt {
struct Vec3 {
    double x, y, z;
    Vec3 operator+(Vec3 b) const { return {x + b.x, y + b.y, z + b.z}; }
    Vec3 operator-(Vec3 b) const { return {x - b.x, y - b.y, z - b.z}; }
    Vec3 operator*(double k) const { return {x * k, y * k, z * k}; }
    Vec3 operator*(Vec3 b) const { return {x * b.x, y * b.y, z * b.z}; }
};
inline double dot(Vec3 a, Vec3 b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
inline Vec3 normalized(Vec3 v) { return v * (1.0 / std::sqrt(dot(v, v))); }
struct Ray { Vec3 origin, direction; };
struct Sphere { Vec3 center; double radius; Vec3 color; Material material{}; };
bool intersect(const Ray& ray, const Sphere& sphere, double& distance);
} // namespace rt
