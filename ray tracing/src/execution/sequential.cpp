#include "execution/sequential.hpp"

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
    // Referencia secuencial: termina un tile antes de comenzar el siguiente.
    // Cada tile escribe una región exclusiva de la imagen.
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
