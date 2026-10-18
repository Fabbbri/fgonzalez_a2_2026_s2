/**
 * @file sequential.cpp
 * Referencia secuencial que recorre los tiles con el índice i.
 * El dominio i = 0, ..., M-1 es el mismo que distribuyen coarse, SMT y CMP.
 * Aquí un único hilo completa cada región antes de pasar a la siguiente.
 * loop_time mide tiempo de pared; render_time descuenta el tiempo de sondeo
 * de eventos medido mediante event_time.
 */
#include "execution/sequential.hpp"

namespace rt {
RenderStats render_sequential(const Scene& scene, const Camera& camera, Image& image,
                              const TileGrid& tiles, const RenderHooks& hooks) {
    using Clock = std::chrono::steady_clock;
    // Acumulador inicializado a cero para el tiempo dedicado a eventos.
    Clock::duration event_time{};
    // Función opcional vacía en modo sin interfaz: evita sondeos innecesarios.
    std::function<bool()> keep_running;
    // Envuelve el callback de aplicación para medir su costo por separado.
    if (hooks.keep_running) keep_running = [&] {
        const auto start = Clock::now();
        const bool open = hooks.keep_running();
        // Acumula únicamente el tiempo consumido al consultar eventos.
        event_time += Clock::now() - start;
        return open;
    };
    RenderStats stats;
    const auto loop_start = Clock::now();
    // Referencia secuencial: termina un tile antes de comenzar el siguiente.
    // Cada tile escribe una región exclusiva de la imagen.
    // i es el índice global del tile. En esta referencia solo lo incrementa
    // el hilo principal; no hay reparto concurrente ni contador atómico.
    for (std::size_t i = 0; i < tiles.size(); ++i) {
        // Obtiene límites exclusivos de la región i, incluidos recortes de borde.
        const Tile tile = tiles.at(i);
        // Guarda el acumulado previo para descontar solo los eventos de este tile.
        const auto events_before = event_time;
        const auto start = Clock::now();
        // Calcula todos los píxeles del rectángulo, salvo cancelación entre píxeles.
        const bool done = render_tile(scene, camera, image, tile, keep_running);
        // Tiempo del tile menos la atención de eventos durante su cálculo.
        stats.render_time += Clock::now() - start - (event_time - events_before);
        // Un tile parcial interrumpe el recorrido y no incrementa completed.
        if (!done) break;
        ++stats.completed;
        if (hooks.tile_completed) hooks.tile_completed(tile);
    }
    // Tiempo de pared del recorrido, incluidos callbacks y coordinación local.
    stats.loop_time = Clock::now() - loop_start;
    return stats;
}
} // namespace rt
