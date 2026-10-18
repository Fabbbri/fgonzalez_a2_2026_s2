#include "render/raytracer.hpp"
#include "render/transport.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace rt {
// Cálculo de píxel común a todos los modelos de ejecución.
// Solo lee escena y cámara; usa variables locales y no lee píxeles vecinos.
// Distintos trabajadores pueden calcular distintos píxeles simultáneamente
// siempre que escena y cámara permanezcan inmutables durante el renderizado.
Pixel render_pixel(const Scene& scene, const Camera& camera, int x, int y) {
    // La semilla depende solo del píxel, nunca del tile ni de su orden.
    const auto coordinate = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(y)) << 32)
                          | static_cast<std::uint32_t>(x);
    Sampler sampler(coordinate ^ scene.settings.seed);
    Vec3 sum{0, 0, 0};
    // Las muestras de cada píxel se calculan en orden, con sum y sampler locales.
    // El reparto de tiles conserva la secuencia aleatoria y el orden de suma.
    for (int sample = 0; sample < scene.settings.samples; ++sample) {
        const double ox = scene.settings.samples == 1 ? 0.5 : sampler.uniform();
        const double oy = scene.settings.samples == 1 ? 0.5 : sampler.uniform();
        sum = sum + trace_path(scene, camera.primary_ray(x, y, ox, oy), sampler);
    }
    const Vec3 average = sum * (1.0 / scene.settings.samples);
    const auto channel = [](double linear) {
        // Gamma aproximada 2: se aplica DESPUÉS de promediar luz lineal.
        const double encoded = std::sqrt(std::clamp(linear, 0.0, 1.0));
        return static_cast<std::uint8_t>(std::lround(encoded * 255.0));
    };
    return {channel(average.x), channel(average.y), channel(average.z)};
}
bool render_tile(const Scene& scene, const Camera& camera, Image& image, Tile tile,
                 const std::function<bool()>& keep_running) {
    TileProgress progress{tile};
    return render_tile_step(scene, camera, image, progress,
                            std::numeric_limits<std::size_t>::max(), keep_running);
}
std::size_t tile_pixels(Tile tile) {
    if (tile.x1 <= tile.x0 || tile.y1 <= tile.y0)
        throw std::invalid_argument("Tile vacío o invertido");
    return static_cast<std::size_t>(tile.x1 - tile.x0) * (tile.y1 - tile.y0);
}
bool render_tile_step(const Scene& scene, const Camera& camera, Image& image,
                      TileProgress& progress, std::size_t budget,
                      const std::function<bool()>& keep_running) {
    if (budget == 0) throw std::invalid_argument("K debe ser positivo");
    const Tile tile = progress.tile;
    const std::size_t total = tile_pixels(tile);
    if (tile.x0 < 0 || tile.y0 < 0 || tile.x1 > image.width() ||
        tile.y1 > image.height() || progress.next > total)
        throw std::invalid_argument("Tile o progreso fuera de imagen");
    const std::size_t end = progress.next + std::min(budget, total - progress.next);
    const std::size_t width = tile.x1 - tile.x0;
    // PARTE QUE EJECUTA CADA TAREA: región exclusiva, muestras locales por píxel.
    // El ancho REAL permite reanudar también los tiles recortados de los bordes.
    while (progress.next < end) {
        if (keep_running && !keep_running()) return false;
        const int x = tile.x0 + static_cast<int>(progress.next % width);
        const int y = tile.y0 + static_cast<int>(progress.next / width);
        image.at(x, y) = render_pixel(scene, camera, x, y);
        ++progress.next; // Solo guardar progreso después del píxel completo.
    }
    return progress.next == total;
}
} // namespace rt
