#include "sequential.hpp"

namespace rt {
RenderStats render_sequential(const Scene& scene, const Camera& camera, Image& image,
                              const TileGrid& tiles, const RenderHooks& hooks) {
    using Clock = std::chrono::steady_clock;
    Clock::duration event_time{};
    std::function<bool()> keep_running;
    if (hooks.keep_running) keep_running = [&] {
        const auto start = Clock::now();
        const bool open = hooks.keep_running();
        event_time += Clock::now() - start;
        return open;
    };
    RenderStats stats;
    const auto loop_start = Clock::now();
    // PARTE QUE VAMOS A PARALELIZAR: el recorrido de tiles de este for.
    // Ejemplo: un trabajador calcula el tile 0 mientras otro calcula el tile 1.
    // Cada tile conserva su región exclusiva de píxeles en la misma imagen.
    // FUTURA PARALELIZACIÓN (1): aquí cambiará la asignación de tiles.
    // Hoy se termina un tile antes de comenzar el siguiente. Los futuros modelos
    // con hilos repartirán tiles; fine-grained intercalará K píxeles por tarea
    // guardando progreso (intercalado no implica paralelismo entre núcleos).
    // No paralelizar este for sin coordinar estadísticas y notificaciones.
    // Conservar este recorrido como baseline; implementar el reparto en otro
    // modelo. Coarse-grained conservará el progreso ante bloqueos simulados;
    // CMP/SMT se evaluarán con sus respectivos trabajadores/configuraciones.
    for (std::size_t i = 0; i < tiles.size(); ++i) {
        const Tile tile = tiles.at(i);
        const auto events_before = event_time;
        const auto start = Clock::now();
        const bool done = render_tile(scene, camera, image, tile, keep_running);
        stats.render_time += Clock::now() - start - (event_time - events_before);
        if (!done) break;
        ++stats.completed;
        if (hooks.tile_completed) hooks.tile_completed(tile);
    }
    stats.loop_time = Clock::now() - loop_start;
    return stats;
}
} // namespace rt
