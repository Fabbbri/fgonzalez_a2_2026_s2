/**
 * @file fine.cpp
 * Planificación cooperativa round-robin de tareas de tiles.
 * Un único hilo real ejecuta todas las tareas: hay intercalado, sin paralelismo
 * entre núcleos. ready contiene estados TileProgress; cada estado conserva
 * el tile y el índice local del siguiente píxel. quantum = K limita píxeles
 * completos por turno, no instrucciones ni ciclos de CPU.
 */
#include "execution/execution.hpp"

#include <deque>
#include <stdexcept>

namespace rt {
// Convierte la enumeración interna a una etiqueta estable para consola y CSV.
const char* model_name(ExecutionModel model) {
    // Selecciona el literal del modelo; un valor ajeno a la enumeración genera error.
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
    // K debe permitir avance: un quantum cero produciría turnos sin progreso.
    if (quantum == 0) throw std::invalid_argument("K debe ser positivo");
    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();
    RenderStats stats;
    // Cola de tareas reanudables; la cabecera representa el siguiente turno.
    std::deque<TileProgress> ready;
    // i enumera el mismo dominio de tiles que distribuyen los modelos paralelos.
    // Cada estado comienza con next = 0 y conserva su rectángulo original.
    for (std::size_t i = 0; i < tiles.size(); ++i) ready.push_back({tiles.at(i)});
    bool running = true;
    // Un solo hilo real simula tareas lógicas: NO hay paralelismo entre núcleos.
    // Callback local que actualiza running al procesar la cancelación del usuario.
    // La ausencia de callback equivale a continuar sin ventana.
    const auto poll = [&] {
        running = !hooks.keep_running || hooks.keep_running();
        return running;
    };
    // La simulación termina por cancelación o al agotar las tareas activas.
    while (running && !ready.empty()) {
        // Copia el estado de la tarea que obtiene el turno; no crea un hilo.
        TileProgress task = ready.front();
        // La retira temporalmente; solo vuelve a la cola si le quedan píxeles.
        ready.pop_front();
        // PLANIFICACIÓN POR TILES: ceder tras K píxeles aunque no haya bloqueo.
        // Avanza hasta K píxeles de task. El siguiente índice queda guardado
        // en task.next para poder reanudar en una ronda posterior.
        const bool complete = render_tile_step(scene, camera, image, task, quantum, poll);
        // Contabiliza un turno cooperativo, aunque el tile termine antes de usar K.
        ++stats.quanta;
        // Una tarea completa sale definitivamente de la ronda y se notifica.
        if (complete) {
            ++stats.completed;
            if (hooks.tile_completed) hooks.tile_completed(task.tile);
        // La reinserción al final implementa round-robin sin requerir un bloqueo.
        } else if (running) ready.push_back(task); // Conservar el próximo píxel.
    }
    stats.loop_time = Clock::now() - start;
    // Tiempo de pared del modelo, incluidos turnos y callbacks; medir headless.
    stats.render_time = stats.loop_time;
    return stats;
}
} // namespace rt
