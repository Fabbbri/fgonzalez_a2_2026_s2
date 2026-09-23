#include "scene.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
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

    // La altura hace que cada esfera toque la superficie del suelo curvo.
    // La separación adicional evita tanto interpenetraciones como contactos
    // demasiado ajustados. El suelo no se compara: es el soporte de todas.
    constexpr double min_gap = 0.16;
    constexpr double glass_gap = 1.2;
    const auto ground_position = [&](double x, double z, double radius) {
        const double distance = ground_radius + radius;
        const double dz = z - ground_center.z;
        return Vec3{x, ground_center.y + std::sqrt(distance*distance - x*x - dz*dz), z};
    };
    const auto clearance = [&](Vec3 center, double radius, MaterialType type) {
        double nearest_margin = std::numeric_limits<double>::infinity();
        for (std::size_t i = 1; i < scene.spheres.size(); ++i) {
            const Sphere& other = scene.spheres[i];
            const Vec3 delta = center - other.center;
            // Dos cristales necesitan más espacio; para el resto rige min_gap.
            const double required = type == MaterialType::glass &&
                                    other.material.type == MaterialType::glass
                                  ? glass_gap : min_gap;
            const double margin = std::sqrt(dot(delta, delta)) - radius - other.radius - required;
            nearest_margin = std::min(nearest_margin, margin);
        }
        return nearest_margin;
    };
    const auto add_sphere = [&](double x, double z, double radius, Vec3 color,
                                Material material = {}) {
        const Vec3 center = ground_position(x, z, radius);
        if (clearance(center, radius, material.type) < 0.0) return false;
        scene.spheres.push_back({center, radius, color, material});
        return true;
    };

    // Mantener intactas las tres protagonistas de la composición actual.
    add_sphere(-3.2, -11.2, 1.0, {0.68, 0.30, 0.16});
    add_sphere(0.0, -11.5, 1.15, {1, 1, 1}, {MaterialType::glass, 0.0, 1.5});
    add_sphere(3.2, -11.2, 1.5, {0.88, 0.72, 0.42}, {MaterialType::metal, 0.03});

    // Antes había 75 decorativas. La semilla fija permite repetir exactamente
    // la escena en cada ejecución y comparar modelos con la misma carga.
    std::mt19937 random(2026);
    const auto sample = [&] { return static_cast<double>(random()) / 4294967296.0; };
    enum class Size { small, medium, large };
    std::vector<Size> sizes;
    sizes.insert(sizes.end(), 49, Size::small);   // ~65 %
    sizes.insert(sizes.end(), 19, Size::medium);  // ~25 %
    sizes.insert(sizes.end(), 7, Size::large);    // ~10 %
    std::shuffle(sizes.begin(), sizes.end(), random);

    // HSV permite elegir tonos vivos sin que los canales RGB produzcan grises por azar.
    const auto hsv_to_rgb = [](double hue, double saturation, double value) -> Vec3 {
        const double h = hue * 6.0;
        const double chroma = value * saturation;
        const double secondary = chroma * (1.0 - std::abs(std::fmod(h, 2.0) - 1.0));
        const double minimum = value - chroma;
        const int sector = static_cast<int>(h);
        if (sector == 0) return {value, secondary + minimum, minimum};
        if (sector == 1) return {secondary + minimum, value, minimum};
        if (sector == 2) return {minimum, value, secondary + minimum};
        if (sector == 3) return {minimum, secondary + minimum, value};
        if (sector == 4) return {secondary + minimum, minimum, value};
        return {value, minimum, secondary + minimum};
    };

    constexpr int max_attempts = 100;
    std::size_t color_index = 0;
    for (const Size size : sizes) {
        double min_radius = 0.12, max_radius = 0.25;
        switch (size) {
        case Size::small:  break;
        case Size::medium: min_radius = 0.25; max_radius = 0.45; break;
        case Size::large:  min_radius = 0.45; max_radius = 0.70; break;
        }
        const double radius = min_radius + (max_radius - min_radius) * sample();
        // Elegir el material antes permite validar también el espacio entre cristales.
        const double choice = sample();
        Material material;
        if (choice > 0.88) material = {MaterialType::glass, 0.0, 1.5};
        else if (choice > 0.65) material = {MaterialType::metal, 0.04 + 0.18 * sample()};
        Vec3 best{};
        double best_score = -1.0;
        // Muestreo de candidatos sin filas. Elegir el que más espacio libre
        // tiene reparte las esferas por la escena sin perder la irregularidad.
        // Las decorativas grandes van más atrás para conservar visibles las
        // protagonistas; las pequeñas pueden ocupar también el primer plano.
        const double x_limit = size == Size::large ? 5.0 : 5.5;
        const double front_z = size == Size::large ? -6.5
                             : size == Size::medium ? -4.0 : -2.5;
        constexpr double back_z = -12.5;
        for (int attempt = 0; attempt < max_attempts; ++attempt) {
            const double x = (2.0 * sample() - 1.0) * x_limit;
            const double z = front_z + (back_z - front_z) * sample();
            const Vec3 candidate = ground_position(x, z, radius);
            const double gap = clearance(candidate, radius, material.type);
            if (gap < 0.0) continue;
            // Favorecer algo de margen en los extremos del encuadre.
            const double edge = std::min(x_limit - std::abs(x),
                                         std::min(front_z - z, z - back_z));
            const double score = std::min(gap, edge);
            if (score > best_score) {
                best = candidate;
                best_score = score;
            }
        }
        if (best_score < 0.0) continue; // Evita bucles sin fin si se llena la zona.

        // El paso áureo reparte los tonos por toda la paleta; un leve azar
        // evita un patrón rígido. Saturación y brillo altos evitan colores apagados.
        // La semilla fija hace reproducible tanto la posición como el color.
        constexpr double golden_step = 0.6180339887498949;
        const double hue = std::fmod(0.17 + color_index * golden_step
                                      + 0.08 * sample(), 1.0);
        const double saturation = 0.72 + 0.23 * sample();
        const double value = 0.82 + 0.16 * sample();
        const Vec3 color = hsv_to_rgb(hue, saturation, value);
        const Vec3 albedo = material.type == MaterialType::diffuse ? color * color : color;
        add_sphere(best.x, best.z, radius, albedo, material);
        ++color_index;
    }

    return scene;
}
} // namespace rt
