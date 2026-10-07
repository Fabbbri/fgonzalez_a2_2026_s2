#include "execution.hpp"

#include <atomic>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace rt {
namespace {
// Motor común de SMT/CMP. Solo cambia la asignación de CPUs a los trabajadores.
// No hay cuantums fine ni bloqueos artificiales coarse en estos modelos.
RenderStats render_pool(const Scene& scene, const Camera& camera, Image& image,
                        const TileGrid& tiles, const std::vector<int>& cpus,
                        const RenderHooks& hooks) {
    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();
    std::atomic<std::size_t> next{0}, finished{0};
    std::atomic<bool> cancel{false};
    std::vector<std::atomic<bool>> complete(tiles.size());
    for (auto& flag : complete) flag.store(false);
    std::vector<bool> notified(tiles.size(), false); // Solo el hilo principal.
    std::vector<std::exception_ptr> errors(cpus.size());
    std::vector<std::thread> workers;
    workers.reserve(cpus.size());
    std::mutex mutex;
    std::condition_variable wake;
    const auto stop = [&] {
        { std::lock_guard<std::mutex> lock(mutex); cancel.store(true); }
        wake.notify_all();
    };
    const auto join = [&] { for (auto& worker : workers) worker.join(); };
    try {
        if (hooks.keep_running && !hooks.keep_running()) stop();
        for (std::size_t i = 0; i < cpus.size() && !cancel.load(); ++i) {
            workers.emplace_back([&, i] {
                try {
                    pin_current_thread(cpus[i]); // Si falla, cancelar y propagar tras join.
                    // PARALELIZACIÓN POR TILES: cada fetch_add entrega un dueño
                    // distinto. Los píxeles del tile se calculan secuencialmente.
                    while (!cancel.load()) {
                        const auto index = next.fetch_add(1);
                        if (index >= tiles.size()) break;
                        const bool done = render_tile(scene, camera, image, tiles.at(index),
                                                      [&] { return !cancel.load(); });
                        // Publicar después de escribir todos los píxeles del tile.
                        complete[index].store(done);
                        if (!done) break;
                    }
                } catch (...) {
                    errors[i] = std::current_exception();
                    stop();
                }
                { std::lock_guard<std::mutex> lock(mutex); ++finished; }
                wake.notify_all();
            });
        }
        while (!cancel.load()) {
            if (hooks.keep_running && !hooks.keep_running()) { stop(); break; }
            for (std::size_t i = 0; i < tiles.size(); ++i) if (!notified[i] && complete[i].load()) {
                notified[i] = true;
                if (hooks.tile_completed) hooks.tile_completed(tiles.at(i));
            }
            if (finished.load() == workers.size()) break;
            std::unique_lock<std::mutex> lock(mutex);
            // Atender eventos aunque no termine ningún tile durante este intervalo.
            wake.wait_for(lock, std::chrono::milliseconds(5), [&] {
                return cancel.load() || finished.load() == workers.size();
            });
        }
    } catch (...) {
        stop();
        join();
        throw;
    }
    join(); // No presentar ni guardar el buffer antes de que termine toda escritura.
    for (const auto& error : errors) if (error) std::rethrow_exception(error);
    RenderStats stats;
    for (std::size_t i = 0; i < tiles.size(); ++i) if (complete[i].load()) {
        ++stats.completed;
        if (!notified[i] && hooks.tile_completed) hooks.tile_completed(tiles.at(i));
    }
    stats.workers = workers.size();
    stats.loop_time = Clock::now() - start;
    stats.render_time = stats.loop_time;
    return stats;
}
} // namespace
RenderStats render_cmp(const Scene& scene, const Camera& camera, Image& image,
                       const TileGrid& tiles, const CpuTopology& topology,
                       std::size_t workers, const RenderHooks& hooks) {
    const auto start = std::chrono::steady_clock::now();
    auto stats = render_pool(scene, camera, image, tiles, cmp_cpus(topology, tiles.size(), workers), hooks);
    stats.loop_time = stats.render_time = std::chrono::steady_clock::now() - start;
    return stats;
}
RenderStats render_smt(const Scene& scene, const Camera& camera, Image& image,
                       const TileGrid& tiles, const CpuTopology& topology,
                       std::size_t workers, const RenderHooks& hooks) {
    const auto start = std::chrono::steady_clock::now();
    auto stats = render_pool(scene, camera, image, tiles, smt_cpus(topology, tiles.size(), workers), hooks);
    stats.loop_time = stats.render_time = std::chrono::steady_clock::now() - start;
    return stats;
}
} // namespace rt
