/**
 * @file raytracer.cpp
 * Cálculo común a todos los modelos: muestras, color y recorrido de píxeles.
 * El paralelismo se aplica fuera de este archivo, al distribuir tiles.
 * Cada llamada escribe solamente la región recibida y utiliza un generador
 * pseudoaleatorio local. Mantener la secuencia de muestras por píxel permite
 * comparar imágenes idénticas aunque cambie el orden de los tiles.
 */
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
    // Empaqueta y en los 32 bits altos y x en los bajos. Las conversiones
    // a enteros sin signo definen el desplazamiento de bits.
    const auto coordinate = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(y)) << 32)
                          | static_cast<std::uint32_t>(x);
    // La combinación XOR con la semilla fija hace reproducible cada píxel
    // sin depender del trabajador, de su CPU ni del orden de ejecución.
    Sampler sampler(coordinate ^ scene.settings.seed);
    // Acumulador RGB privado del píxel; no existe una suma compartida entre hilos.
    Vec3 sum{0, 0, 0};
    // Las muestras de cada píxel se calculan en orden, con sum y sampler locales.
    // El reparto de tiles conserva la secuencia aleatoria y el orden de suma.
    for (int sample = 0; sample < scene.settings.samples; ++sample) {
        // Desplazamiento subpíxel horizontal: centro si n = 1, uniforme si n > 1.
        const double ox = scene.settings.samples == 1 ? 0.5 : sampler.uniform();
        // Desplazamiento vertical independiente para el antialiasing por muestreo.
        const double oy = scene.settings.samples == 1 ? 0.5 : sampler.uniform();
        // Genera el rayo primario y acumula la contribución de toda su trayectoria.
        // El orden fijo de suma evita diferencias por asociatividad de punto flotante.
        sum = sum + trace_path(scene, camera.primary_ray(x, y, ox, oy), sampler);
    }
    // Estimador Monte Carlo: promedio de las muestras en espacio lineal.
    const Vec3 average = sum * (1.0 / scene.settings.samples);
    // Transformación local de un canal lineal a un byte RGB con gamma aproximada.
    const auto channel = [](double linear) {
        // Gamma aproximada 2: se aplica DESPUÉS de promediar luz lineal.
        // clamp restringe energía al intervalo representable; sqrt aplica gamma 2.
        const double encoded = std::sqrt(std::clamp(linear, 0.0, 1.0));
        // Escala a [0,255], redondea al entero cercano y convierte a ocho bits.
        return static_cast<std::uint8_t>(std::lround(encoded * 255.0));
    };
    return {channel(average.x), channel(average.y), channel(average.z)};
}
bool render_tile(const Scene& scene, const Camera& camera, Image& image, Tile tile,
                 const std::function<bool()>& keep_running) {
    // Estado privado con next = 0; render_tile ejecuta el mismo núcleo reanudable.
    TileProgress progress{tile};
    // El presupuesto máximo permite completar la región sin turnos de quantum.
    return render_tile_step(scene, camera, image, progress,
                            std::numeric_limits<std::size_t>::max(), keep_running);
}
std::size_t tile_pixels(Tile tile) {
    // Rechaza regiones vacías o con extremos invertidos antes de multiplicar áreas.
    if (tile.x1 <= tile.x0 || tile.y1 <= tile.y0)
        throw std::invalid_argument("Tile vacío o invertido");
    // Área del rectángulo real, que puede ser menor en el borde de la imagen.
    return static_cast<std::size_t>(tile.x1 - tile.x0) * (tile.y1 - tile.y0);
}
bool render_tile_step(const Scene& scene, const Camera& camera, Image& image,
                      TileProgress& progress, std::size_t budget,
                      const std::function<bool()>& keep_running) {
    if (budget == 0) throw std::invalid_argument("K debe ser positivo");
    // Copia los límites de la tarea; progress.next conserva el avance entre llamadas.
    const Tile tile = progress.tile;
    // Cantidad de píxeles de esta tarea, independiente del total de tiles M.
    const std::size_t total = tile_pixels(tile);
    // Protege los accesos al buffer y valida que el progreso no excede el área.
    if (tile.x0 < 0 || tile.y0 < 0 || tile.x1 > image.width() ||
        tile.y1 > image.height() || progress.next > total)
        throw std::invalid_argument("Tile o progreso fuera de imagen");
    // Fin exclusivo del turno: avanza min(K, píxeles restantes). Restar antes
    // de sumar evita exceder total, incluso con un presupuesto muy grande.
    const std::size_t end = progress.next + std::min(budget, total - progress.next);
    // Ancho efectivo del tile recortado; no usa el ancho nominal de la grilla.
    const std::size_t width = tile.x1 - tile.x0;
    // PARTE QUE EJECUTA CADA TAREA: región exclusiva, muestras locales por píxel.
    // El ancho REAL permite reanudar también los tiles recortados de los bordes.
    // Bucle local secuencial de píxeles. La paralelización está en el índice
    // global del tile, no en progress.next, que pertenece a una sola tarea.
    while (progress.next < end) {
        // Sondea entre píxeles completos: no interrumpe un rebote a la mitad.
        if (keep_running && !keep_running()) return false;
        // Residuo del índice local: columna dentro del tile, trasladada por x0.
        const int x = tile.x0 + static_cast<int>(progress.next % width);
        // Cociente del índice local: fila dentro del tile, trasladada por y0.
        const int y = tile.y0 + static_cast<int>(progress.next / width);
        // Escritura exclusiva del dueño del tile. Image ya tiene tamaño fijo;
        // ningún otro tile contiene este par (x,y). Puede existir contención de
        // caché entre regiones próximas, pero no escritura del mismo píxel.
        image.at(x, y) = render_pixel(scene, camera, x, y);
        ++progress.next; // Solo guardar progreso después del píxel completo.
    }
    // true significa región terminada; false puede ser fin del quantum o cancelación.
    return progress.next == total;
}
} // namespace rt
