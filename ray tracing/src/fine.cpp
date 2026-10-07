#include "execution.hpp"

#include <deque>
#include <stdexcept>

namespace rt {
const char* model_name(ExecutionModel model) {
    switch (model) {
    case ExecutionModel::sequential: return "sequential";
    case ExecutionModel::fine: return "fine";
    case ExecutionModel::coarse: return "coarse";
    case ExecutionModel::smt: return "smt";
    case ExecutionModel::cmp: return "cmp";
    }
    throw std::invalid_argument("Modelo inválido");
}
RenderStats render_fine(const Scene& scene, const Camera& camera, Image& image,
                        const TileGrid& tiles, std::size_t quantum,
                        const RenderHooks& hooks) {
    if (quantum == 0) throw std::invalid_argument("K debe ser positivo");
    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();
    RenderStats stats;
    std::deque<TileProgress> ready;
    for (std::size_t i = 0; i < tiles.size(); ++i) ready.push_back({tiles.at(i)});
    bool running = true;
    // Un solo hilo real simula tareas lógicas: NO hay paralelismo entre núcleos.
    const auto poll = [&] {
        running = !hooks.keep_running || hooks.keep_running();
        return running;
    };
    while (running && !ready.empty()) {
        TileProgress task = ready.front();
        ready.pop_front();
        // PLANIFICACIÓN POR TILES: ceder tras K píxeles aunque no haya bloqueo.
        const bool complete = render_tile_step(scene, camera, image, task, quantum, poll);
        ++stats.quanta;
        if (complete) {
            ++stats.completed;
            if (hooks.tile_completed) hooks.tile_completed(task.tile);
        } else if (running) ready.push_back(task); // Conservar el próximo píxel.
    }
    stats.loop_time = Clock::now() - start;
    // Tiempo de pared del modelo, incluidos turnos y callbacks; medir headless.
    stats.render_time = stats.loop_time;
    return stats;
}
} // namespace rt
