#include "raytracer.hpp"
#include "transport.hpp"
#include <algorithm>
#include <cmath>

namespace rt {
// FUTURA PARALELIZACIÓN (2): cálculo independiente que compartirán las tareas.
// Solo lee escena y cámara; usa variables locales y no lee píxeles vecinos.
// Distintos trabajadores pueden calcular distintos píxeles simultáneamente
// siempre que escena y cámara permanezcan inmutables durante el renderizado.
Pixel render_pixel(const Scene& scene, const Camera& camera, int x, int y) {
    // La semilla depende solo del píxel, nunca del tile ni de su orden.
    const auto coordinate = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(y)) << 32)
                          | static_cast<std::uint32_t>(x);
    Sampler sampler(coordinate ^ scene.settings.seed);
    Vec3 sum{0, 0, 0};
    // FUTURA PARALELIZACIÓN: cada trabajador completa las muestras de su píxel
    // en este orden. sum y sampler son locales y no deben compartirse con otro.
    // Repartiremos tiles, manteniendo las muestras internas secuenciales para
    // conservar tanto la secuencia aleatoria como el orden de suma de colores.
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
    // FUTURA PARALELIZACIÓN (3): este es el trabajo de una región asignada.
    // Tiles disjuntos escriben píxeles distintos del buffer, que ya está reservado;
    // no requieren un mutex por píxel si no se redimensiona ni se lee antes de tiempo.
    // Para fine-grained habrá que conservar el próximo píxel y retornar tras K;
    // K cuenta píxeles COMPLETOS, con todas sus muestras y rebotes. Bastará guardar
    // el índice del siguiente píxel; un píxel terminado no necesita conservar su RNG.
    // hoy estos bucles terminan el tile completo y no guardan progreso reanudable.
    // El callback actual llama a SDL: no trasladarlo a hilos trabajadores.
    for (int y = tile.y0; y < tile.y1; ++y) {
        for (int x = tile.x0; x < tile.x1; ++x) {
            // Cada píxel tiene muchas muestras: atender eventos antes de cada uno.
            if (keep_running && !keep_running()) return false;
            // TRABAJO QUE EJECUTARÁ CADA TRABAJADOR: calcular y guardar un píxel
            // de su tile. El reparto ocurre en el recorrido de sequential.cpp;
            // dentro de cada tile se conserva este orden de filas y columnas.
            image.at(x, y) = render_pixel(scene, camera, x, y);
        }
    }
    return true;
}
} // namespace rt
