#include "scene.hpp"
#include <cmath>
#include <random>

namespace rt {
Scene default_scene() {
    Scene scene;
    scene.background = {0.48, 0.66, 0.95};

    // Composición inspirada en "A Final Render" de Ray Tracing in One Weekend:
    // https://raytracing.github.io/books/RayTracingInOneWeekend.html
    // Materiales mates, metálicos y vidrio; el cálculo sigue siendo secuencial.
    const Vec3 ground_center{0, -1001, -6};
    constexpr double ground_radius = 1000;
    scene.spheres.push_back({ground_center, ground_radius, {0.42, 0.44, 0.46}});

    // Coloca una esfera tangente al suelo y evita interpenetraciones.
    const auto add_sphere = [&](double x, double z, double radius, Vec3 color, Material material = {}) {
        const double distance = ground_radius + radius;
        const double dz = z - ground_center.z;
        const double y = ground_center.y + std::sqrt(distance*distance - x*x - dz*dz);
        const Vec3 center{x, y, z};
        for (std::size_t i = 1; i < scene.spheres.size(); ++i) {
            const Vec3 delta = center - scene.spheres[i].center;
            const double separation = radius + scene.spheres[i].radius + 0.12;
            if (dot(delta, delta) < separation * separation) return;
        }
        scene.spheres.push_back({center, radius, color, material});
    };

    // Tres protagonistas separadas, en la zona posterior del conjunto.
    add_sphere(-3.0, -8.0, 0.85, {0.68, 0.30, 0.16});
    add_sphere(0.0, -8.5, 1.00, {1, 1, 1}, {MaterialType::glass, 0.0, 1.5});
    add_sphere(3.0, -8.0, 0.85, {0.88, 0.72, 0.42}, {MaterialType::metal, 0.03});

    // Semilla fija: el aspecto es irregular, pero la escena se repite exactamente
    // para comparar tiempos. La aleatoriedad solo se usa al crear la escena.
    std::mt19937 random(2026);
    const auto sample = [&] { return static_cast<double>(random()) / 4294967296.0; };
    constexpr int rows = 10;
    constexpr int columns = 13;
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            const double x = (column - (columns - 1) / 2.0) * 1.1 + 0.5 * (sample() - 0.5);
            const double z = -1.0 - row * 1.4 + 0.5 * (sample() - 0.5);
            const double radius = 0.18 + 0.15 * sample();
            const Vec3 color{0.18 + 0.72 * sample(), 0.18 + 0.72 * sample(),
                             0.18 + 0.72 * sample()};
            const double choice = sample();
            Material material;
            if (choice > 0.88) material = {MaterialType::glass, 0.0, 1.5};
            else if (choice > 0.65) material = {MaterialType::metal, 0.04 + 0.18 * sample()};
            // Reflectancia lineal más saturada para los mates; el metal conserva
            // una reflectancia alta para que sus reflejos no queden negros.
            const Vec3 albedo = material.type == MaterialType::diffuse ? color * color : color;
            add_sphere(x, z, radius, albedo, material);
        }
    }
    return scene;
}
} // namespace rt
