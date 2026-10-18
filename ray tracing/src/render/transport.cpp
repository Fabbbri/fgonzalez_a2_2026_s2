/**
 * @file transport.cpp
 * Trazado iterativo de trayectorias con materiales difusos, metálicos y vidrio.
 * El rebote siguiente depende del impacto anterior: esta dependencia mantiene
 * el bucle de rebotes secuencial. La independencia entre tiles proporciona
 * el paralelismo. La luz del cielo es la fuente de iluminación del modelo.
 */
#include "render/transport.hpp"

#include <algorithm>
#include <limits>

namespace rt {
Vec3 reflect(Vec3 direction, Vec3 normal) {
    // Reflexión especular: elimina dos veces la proyección sobre la normal unitaria.
    return direction - normal * (2.0 * dot(direction, normal));
}
bool refract(Vec3 direction, Vec3 normal, double eta, Vec3& transmitted) {
    // Snell: eta*sin(theta_entrada) = sin(theta_salida).
    // Coseno de incidencia acotado contra errores de redondeo.
    const double cosine = std::clamp(-dot(direction, normal), 0.0, 1.0);
    // Componente tangencial transmitida, escalada por la razón de índices eta.
    const Vec3 tangent = (direction + normal * cosine) * eta;
    // La longitud de la dirección transmitida debe ser uno: queda 1-|tangente|².
    const double normal_squared = 1.0 - dot(tangent, tangent);
    // Un valor negativo indica reflexión interna total; no existe rayo refractado.
    if (normal_squared < 0.0) return false;
    // Reconstruye la componente normal con signo hacia el medio de salida.
    transmitted = tangent - normal * std::sqrt(normal_squared);
    return true;
}

Vec3 trace_path(const Scene& scene, Ray ray, Sampler& sampler) {
    Vec3 throughput{1, 1, 1}; // Fracción de luz que sobrevive a cada interacción.
    // El trabajador dueño del píxel calcula la trayectoria completa.
    // Los rebotes de UNA trayectoria dependen del impacto anterior: este bucle
    // permanece secuencial. Distintos trabajadores sí pueden seguir trayectorias
    // de píxeles diferentes a la vez, leyendo la misma escena inmutable.
    // Cuenta impactos hasta el límite; una trayectoria que escapa antes obtiene cielo.
    for (int bounce = 0; bounce <= scene.settings.max_bounces; ++bounce) {
        // Distancia mínima privada: infinito permite aceptar el primer impacto.
        double nearest = std::numeric_limits<double>::infinity();
        // Puntero de solo lectura al objeto más cercano; nullptr indica cielo.
        const Sphere* hit = nullptr;
        // La búsqueda de impactos se mantiene local a cada rayo. El reparto
        // se realiza por tiles, no por esfera ni por rebote.
        // Búsqueda lineal de intersección más próxima; cada rayo consulta toda la escena.
        for (const Sphere& sphere : scene.spheres) {
            // Variable de salida local para intersect; nunca compartida con otros hilos.
            double distance;
            if (intersect(ray, sphere, distance) && distance < nearest) {
                // Actualiza el mínimo y el objeto asociado como una pareja consistente local.
                nearest = distance;
                hit = &sphere;
            }
        }
        // La ausencia de impactos termina la trayectoria con radiancia del entorno.
        if (!hit) {
            // Luz de entorno: mezcla de horizonte blanco y cielo azul.
            const double blend = std::clamp(0.5 * (ray.direction.y + 1.0), 0.0, 1.0);
            return throughput * (scene.horizon * (1.0 - blend) + scene.background * blend);
        }
        // Un impacto en el límite termina sin radiancia adicional.
        if (bounce == scene.settings.max_bounces) return {0, 0, 0};

        // Punto de impacto obtenido al evaluar O+tD.
        const Vec3 point = ray.origin + ray.direction * nearest;
        // Normal exterior unitaria de la esfera: (punto-centro)/radio.
        const Vec3 outward = (point - hit->center) * (1.0 / hit->radius);
        // Producto negativo indica entrada; positivo indica salida de la esfera.
        const bool entering = dot(ray.direction, outward) < 0.0;
        // Orienta la normal contra el rayo para usar las mismas fórmulas en ambos lados.
        const Vec3 normal = entering ? outward : outward * -1.0;
        // Dirección del segmento siguiente; se selecciona según material.
        Vec3 next;
        switch (hit->material.type) {
        case MaterialType::diffuse:
            // N + vector uniforme produce un hemisferio ponderado por coseno.
            // Las trayectorias bloqueadas por otros objetos explican las sombras
            // de contacto; no se añade una mancha ni un ambiente constante.
            next = normal + sampler.unit_vector();
            // Evita normalizar una dispersión casi nula cuando se cancelan los vectores.
            if (dot(next, next) < 1e-12) next = normal;
            // Producto RGB componente a componente: atenuación de la trayectoria.
            throughput = throughput * hit->color;
            break;
        case MaterialType::metal:
            next = reflect(ray.direction, normal)
                 + sampler.unit_vector() * hit->material.roughness;
            // Una reflexión metálica que apunta dentro de la superficie se absorbe.
            if (dot(next, normal) <= 0.0) return {0, 0, 0};
            throughput = throughput * hit->color;
            break;
        case MaterialType::glass: {
            // Razón n_entrada/n_salida: aire a vidrio al entrar y vidrio a aire al salir.
            const double eta = entering ? 1.0 / hit->material.refractive_index
                                        : hit->material.refractive_index;
            Vec3 transmitted;
            // Determina si Snell admite transmisión o exige reflexión interna total.
            const bool can_transmit = refract(ray.direction, normal, eta, transmitted);
            // Schlick: probabilidad de reflexión, mayor al mirar de lado.
            const double cosine = std::clamp(-dot(ray.direction, normal), 0.0, 1.0);
            // Coeficiente de Fresnel aproximado a incidencia normal antes de elevar al cuadrado.
            double base = (1.0 - eta) / (1.0 + eta);
            base *= base;
            // Aproximación de Schlick: aumenta la probabilidad de reflexión a incidencia rasante.
            const double reflection = base + (1.0 - base) * std::pow(1.0 - cosine, 5);
            // Selección estocástica entre reflexión y refracción con el generador local.
            next = (!can_transmit || sampler.uniform() < reflection)
                 ? reflect(ray.direction, normal) : transmitted;
            // Vidrio transparente sin absorción: no se multiplica por el color.
            break;
        }
        }
        // Conserva direcciones unitarias para el cálculo óptico del siguiente segmento.
        next = normalized(next);
        // Desplazar al lado de salida evita reimpactar la misma superficie por
        // redondeo. Para vidrio refractado se desplaza hacia dentro de la esfera.
        // Signo del desplazamiento respecto a la normal exterior según la salida elegida.
        const double side = dot(next, outward) >= 0.0 ? 1.0 : -1.0;
        // Actualiza origen y dirección para el siguiente rebote, con sesgo geométrico 1e-4.
        ray = {point + outward * (side * 1e-4), next};
    }
    return {0, 0, 0};
}
} // namespace rt
