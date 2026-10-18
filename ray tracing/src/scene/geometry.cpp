#include "scene/geometry.hpp"

namespace rt {
// Cálculo geométrico reutilizado por los futuros trabajadores para cada rayo.
// La unidad de reparto será el tile (ver sequential.cpp), no esta fórmula.
// Solo lee ray y sphere; distance debe ser una variable local de cada llamada,
// como ocurre en render_pixel, para que los trabajadores no compartan su salida.
bool intersect(const Ray& ray, const Sphere& sphere, double& distance) {
    // |O + tD - C|² = r² -> a*t² + 2*half_b*t + c = 0.
    const Vec3 oc = ray.origin - sphere.center;
    const double a = dot(ray.direction, ray.direction);
    const double half_b = dot(oc, ray.direction);
    const double c = dot(oc, oc) - sphere.radius*sphere.radius;
    const double discriminant = half_b*half_b - a*c;
    if (discriminant < 0.0) return false;
    const double root = std::sqrt(discriminant);
    constexpr double epsilon = 1e-6;
    double t = (-half_b - root) / a;
    // La segunda raíz permite ver la salida si el origen está dentro.
    if (t <= epsilon) t = (-half_b + root) / a;
    if (t <= epsilon) return false;
    distance = t;
    return true;
}

} // namespace rt
