/**
 * @file geometry.cpp
 * Intersección analítica entre un rayo y una esfera.
 * Resuelve la ecuación cuadrática y selecciona la primera raíz positiva
 * por encima de epsilon. El resultado distance es privado de cada trayectoria.
 */
#include "scene/geometry.hpp"

namespace rt {
// Operación pura sobre rayo y esfera; cada trayectoria aporta su propia distance.
bool intersect(const Ray& ray, const Sphere& sphere, double& distance) {
    // |O + tD - C|² = r² -> a*t² + 2*half_b*t + c = 0.
    // Vector O-C desde el centro de la esfera al origen del rayo.
    const Vec3 oc = ray.origin - sphere.center;
    // Coeficiente D·D; conserva validez aunque la dirección no sea unitaria.
    const double a = dot(ray.direction, ray.direction);
    // Mitad del coeficiente lineal: (O-C)·D.
    const double half_b = dot(oc, ray.direction);
    // Término constante: distancia al centro al cuadrado menos radio al cuadrado.
    const double c = dot(oc, oc) - sphere.radius*sphere.radius;
    // Discriminante reducido; negativo implica ausencia de intersección real.
    const double discriminant = half_b*half_b - a*c;
    if (discriminant < 0.0) return false;
    // Raíz del discriminante para calcular las dos soluciones paramétricas.
    const double root = std::sqrt(discriminant);
    // Umbral positivo que evita impactos a distancia numéricamente nula.
    constexpr double epsilon = 1e-6;
    // La raíz con signo menos es la más próxima para a positivo.
    double t = (-half_b - root) / a;
    // La segunda raíz permite ver la salida si el origen está dentro.
    if (t <= epsilon) t = (-half_b + root) / a;
    // Ninguna raíz queda delante del origen por encima del umbral.
    if (t <= epsilon) return false;
    // Entrega la distancia paramétrica al llamante solo cuando existe impacto válido.
    distance = t;
    return true;
}

} // namespace rt
