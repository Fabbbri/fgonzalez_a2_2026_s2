#include "transport.hpp"

#include <algorithm>
#include <limits>

namespace rt {
Vec3 reflect(Vec3 direction, Vec3 normal) {
    return direction - normal * (2.0 * dot(direction, normal));
}
bool refract(Vec3 direction, Vec3 normal, double eta, Vec3& transmitted) {
    // Snell: eta*sin(theta_entrada) = sin(theta_salida).
    const double cosine = std::clamp(-dot(direction, normal), 0.0, 1.0);
    const Vec3 tangent = (direction + normal * cosine) * eta;
    const double normal_squared = 1.0 - dot(tangent, tangent);
    if (normal_squared < 0.0) return false;
    transmitted = tangent - normal * std::sqrt(normal_squared);
    return true;
}

Vec3 trace_path(const Scene& scene, Ray ray, Sampler& sampler) {
    Vec3 throughput{1, 1, 1}; // Fracción de luz que sobrevive a cada interacción.
    // FUTURA PARALELIZACIÓN: este cálculo lo ejecutará el dueño del píxel.
    // Los rebotes de UNA trayectoria dependen del impacto anterior: este bucle
    // permanece secuencial. Distintos trabajadores sí pueden seguir trayectorias
    // de píxeles diferentes a la vez, leyendo la misma escena inmutable.
    for (int bounce = 0; bounce <= scene.settings.max_bounces; ++bounce) {
        double nearest = std::numeric_limits<double>::infinity();
        const Sphere* hit = nullptr;
        // La búsqueda de impactos se mantiene local a cada rayo. El reparto
        // previsto es por tiles, no un hilo por esfera ni por rebote.
        for (const Sphere& sphere : scene.spheres) {
            double distance;
            if (intersect(ray, sphere, distance) && distance < nearest) {
                nearest = distance;
                hit = &sphere;
            }
        }
        if (!hit) {
            // Luz de entorno: mezcla de horizonte blanco y cielo azul.
            const double blend = std::clamp(0.5 * (ray.direction.y + 1.0), 0.0, 1.0);
            return throughput * (scene.horizon * (1.0 - blend) + scene.background * blend);
        }
        if (bounce == scene.settings.max_bounces) return {0, 0, 0};

        const Vec3 point = ray.origin + ray.direction * nearest;
        const Vec3 outward = (point - hit->center) * (1.0 / hit->radius);
        const bool entering = dot(ray.direction, outward) < 0.0;
        const Vec3 normal = entering ? outward : outward * -1.0;
        Vec3 next;
        switch (hit->material.type) {
        case MaterialType::diffuse:
            // N + vector uniforme produce un hemisferio ponderado por coseno.
            // Las trayectorias bloqueadas por otros objetos explican las sombras
            // de contacto; no se añade una mancha ni un ambiente constante.
            next = normal + sampler.unit_vector();
            if (dot(next, next) < 1e-12) next = normal;
            throughput = throughput * hit->color;
            break;
        case MaterialType::metal:
            next = reflect(ray.direction, normal)
                 + sampler.unit_vector() * hit->material.roughness;
            if (dot(next, normal) <= 0.0) return {0, 0, 0};
            throughput = throughput * hit->color;
            break;
        case MaterialType::glass: {
            const double eta = entering ? 1.0 / hit->material.refractive_index
                                        : hit->material.refractive_index;
            Vec3 transmitted;
            const bool can_transmit = refract(ray.direction, normal, eta, transmitted);
            // Schlick: probabilidad de reflexión, mayor al mirar de lado.
            const double cosine = std::clamp(-dot(ray.direction, normal), 0.0, 1.0);
            double base = (1.0 - eta) / (1.0 + eta);
            base *= base;
            const double reflection = base + (1.0 - base) * std::pow(1.0 - cosine, 5);
            next = (!can_transmit || sampler.uniform() < reflection)
                 ? reflect(ray.direction, normal) : transmitted;
            // Vidrio transparente sin absorción: no se multiplica por el color.
            break;
        }
        }
        next = normalized(next);
        // Desplazar al lado de salida evita reimpactar la misma superficie por
        // redondeo. Para vidrio refractado se desplaza hacia dentro de la esfera.
        const double side = dot(next, outward) >= 0.0 ? 1.0 : -1.0;
        ray = {point + outward * (side * 1e-4), next};
    }
    return {0, 0, 0};
}
} // namespace rt
