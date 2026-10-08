#include "execution/execution.hpp"

#include <atomic>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace rt {
RenderStats render_coarse(const Scene& scene, const Camera& camera, Image& image,
                          const TileGrid& tiles, int block_interval_ms,
                          int block_duration_ms, const RenderHooks& hooks,
                          std::size_t worker_limit) {
    if (block_interval_ms < 0 || block_duration_ms < 0)
        throw std::invalid_argument("Los tiempos de bloqueo no pueden ser negativos");
    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();
    const auto interval = std::chrono::milliseconds(block_interval_ms);
    const auto duration = std::chrono::milliseconds(block_duration_ms);
    const std::size_t count = tiles.size();
    if (worker_limit > count) throw std::invalid_argument("Más trabajadores que tiles");
    const std::size_t worker_count = worker_limit == 0 ? count : worker_limit;
    std::atomic<bool> cancel{false};
    std::atomic<std::size_t> finished{0}, blocks{0};
    std::atomic<std::size_t> next_tile{0};
    std::vector<std::atomic<bool>> complete(count);
    for (auto& flag : complete) flag.store(false);
    std::vector<std::exception_ptr> errors(count);
    std::vector<std::thread> workers;
    workers.reserve(worker_count);
    std::mutex mutex;
    std::condition_variable wake;
    RenderStats stats;
    std::vector<bool> notified(count, false); // Solo lo usa el hilo principal.
    const auto stop = [&] {
        { std::lock_guard<std::mutex> lock(mutex); cancel.store(true); }
        wake.notify_all(); // Despertar esperas largas, sin perder la notificación.
    };
    const auto join = [&] { for (auto& worker : workers) worker.join(); };
    try {
        // Los callbacks pertenecen al hilo principal, incluido este sondeo inicial.
        if (hooks.keep_running && !hooks.keep_running()) stop();
        if (!cancel.load()) for (std::size_t i = 0; i < worker_count && !cancel.load(); ++i) {
            // PARALELIZACIÓN POR TILES: Ti es dueño exclusivo del rectángulo Bi.
            workers.emplace_back([&, i] {
                try {
                    auto last_resume = Clock::now();
                    const auto keep_running = [&] {
                        if (cancel.load()) return false;
                        // El reloj provoca una ESPERA; no alterna tareas por K.
                        if (block_interval_ms > 0 && Clock::now() - last_resume >= interval) {
                            ++blocks;
                            std::unique_lock<std::mutex> lock(mutex);
                            wake.wait_for(lock, duration, [&] { return cancel.load(); });
                            last_resume = Clock::now();
                        }
                        return !cancel.load();
                    };
                    // Por defecto Ti recibe Bi. --threads activa una variante con P
                    // trabajadores para medir escalabilidad con la MISMA partición.
                    std::size_t tile_index = worker_limit == 0 ? i : next_tile.fetch_add(1);
                    while (tile_index < count && !cancel.load()) {
                        const bool done = render_tile(scene, camera, image, tiles.at(tile_index), keep_running);
                        complete[tile_index].store(done);
                        if (!done || worker_limit == 0) break;
                        tile_index = next_tile.fetch_add(1); // Un único dueño por tile.
                    }
                } catch (...) {
                    errors[i] = std::current_exception();
                    stop();
                }
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    ++finished;
                }
                wake.notify_all();
            });
        }
        while (!cancel.load()) {
            if (hooks.keep_running && !hooks.keep_running()) {
                stop();
                break;
            }
            for (std::size_t i = 0; i < count; ++i) if (!notified[i] && complete[i].load()) {
                notified[i] = true;
                if (hooks.tile_completed) hooks.tile_completed(tiles.at(i));
            }
            if (finished.load() == worker_count) break;
            std::unique_lock<std::mutex> lock(mutex);
            // Sondear SDL en el principal incluso si todos los trabajadores están bloqueados.
            wake.wait_for(lock, std::chrono::milliseconds(5), [&] {
                return cancel.load() || finished.load() == worker_count;
            });
        }
    } catch (...) {
        // También cubre fallos al crear un hilo y excepciones en callbacks SDL.
        stop();
        join();
        throw;
    }
    if (cancel.load()) wake.notify_all();
    join(); // Antes de leer el resultado completo, presentar o devolver excepciones.
    for (const auto& error : errors) if (error) std::rethrow_exception(error);
    for (std::size_t i = 0; i < count; ++i) if (complete[i].load()) {
        ++stats.completed;
        if (!notified[i] && hooks.tile_completed) hooks.tile_completed(tiles.at(i));
    }
    stats.workers = workers.size();
    stats.blocks = blocks.load();
    stats.loop_time = Clock::now() - start;
    // Tiempo de pared: incluye creación, coordinación, bloqueos y join.
    stats.render_time = stats.loop_time;
    return stats;
}
} // namespace rt
