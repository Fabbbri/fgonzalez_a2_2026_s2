/**
 * @file coarse.cpp
 * Multihilo de grano grueso con bloqueos temporizados e interrumpibles.
 *
 * La dimensión paralelizada es el índice del tile, no los rebotes del rayo.
 * count = M representa la cantidad de tiles; worker_count = P, la de hilos.
 * Con worker_limit == 0, el trabajador i recibe el tile i una sola vez.
 * Con worker_limit > 0, next_tile.fetch_add(1) asigna índices exclusivos;
 * tile_index es privado de cada trabajador y selecciona su región de image.
 * El trabajador mantiene la propiedad del tile incluso mientras está bloqueado.
 *
 * El temporizador modela una espera costosa; no alterna tiles por quantum.
 * El SO conserva la facultad de desplanificar hilos fuera de esas esperas.
 * Mutex y variable de condición coordinan cancelación y esperas; las escrituras
 * de píxeles no requieren un mutex porque los tiles no se superponen.
 */
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
    // Validación de los tiempos físicos del modelo; no admite duraciones negativas.
    if (block_interval_ms < 0 || block_duration_ms < 0)
        throw std::invalid_argument("Los tiempos de bloqueo no pueden ser negativos");
    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();
    // interval define cuánto tiempo de pared transcurre antes de simular bloqueo.
    const auto interval = std::chrono::milliseconds(block_interval_ms);
    // duration es el tiempo de suspensión pedido para cada bloqueo.
    const auto duration = std::chrono::milliseconds(block_duration_ms);
    // M: cantidad fija de regiones; variar P no cambia estos límites geométricos.
    const std::size_t count = tiles.size();
    if (worker_limit > count) throw std::invalid_argument("Más trabajadores que tiles");
    // P = M en el modo un hilo por tile; P = worker_limit en el grupo limitado.
    const std::size_t worker_count = worker_limit == 0 ? count : worker_limit;
    std::atomic<bool> cancel{false};
    // finished cuenta salidas de hilos; blocks acumula esperas de todos ellos.
    std::atomic<std::size_t> finished{0}, blocks{0};
    // Contador de reparto dinámico para --threads P. Es la variable compartida
    // que distribuye el dominio de índices de tile mediante fetch_add.
    std::atomic<std::size_t> next_tile{0};
    // Una bandera atómica por región; solo su dueño publica su finalización.
    std::vector<std::atomic<bool>> complete(count);
    for (auto& flag : complete) flag.store(false);
    // Casillas de errores separadas por trabajador; P nunca supera M.
    std::vector<std::exception_ptr> errors(count);
    std::vector<std::thread> workers;
    workers.reserve(worker_count);
    std::mutex mutex;
    std::condition_variable wake;
    RenderStats stats;
    std::vector<bool> notified(count, false); // Solo lo usa el hilo principal.
    // La cancelación se publica bajo el mutex que usa el predicado de espera.
    // notify_all despierta tanto bloqueos simulados como la supervisión.
    const auto stop = [&] {
        { std::lock_guard<std::mutex> lock(mutex); cancel.store(true); }
        wake.notify_all(); // Despertar esperas largas, sin perder la notificación.
    };
    // Reunión de trabajadores: sincroniza el buffer antes de leerlo o devolverlo.
    const auto join = [&] { for (auto& worker : workers) worker.join(); };
    try {
        // Los callbacks pertenecen al hilo principal, incluido este sondeo inicial.
        if (hooks.keep_running && !hooks.keep_running()) stop();
        if (!cancel.load()) for (std::size_t i = 0; i < worker_count && !cancel.load(); ++i) {
            // PARALELIZACIÓN POR TILES: Ti es dueño exclusivo del rectángulo Bi.
            // Captura i por valor para fijar identidad; el resto del contexto se comparte
            // por referencia y mantiene su vida útil hasta finalizar todos los join.
            workers.emplace_back([&, i] {
                try {
                    // Marca privada de la última reanudación de este trabajador.
                    auto last_resume = Clock::now();
                    // Sondeo entre píxeles que combina cancelación y simulación de bloqueo.
                    // No cambia de tile: el trabajador conserva la región mientras espera.
                    const auto keep_running = [&] {
                        if (cancel.load()) return false;
                        // El reloj provoca una ESPERA; no alterna tareas por K.
                        // El valor cero desactiva bloqueos. La comparación usa tiempo de pared,
                        // no un contador de instrucciones ni de fallos de caché reales.
                        if (block_interval_ms > 0 && Clock::now() - last_resume >= interval) {
                            // Incremento atómico del total compartido de bloqueos observados.
                            ++blocks;
                            std::unique_lock<std::mutex> lock(mutex);
                            // Suspensión interrumpible: libera el mutex; retorna por plazo o cancelación.
                            // Una notificación sin cancelación no satisface el predicado de salida.
                            wake.wait_for(lock, duration, [&] { return cancel.load(); });
                            // Reinicia la referencia temporal después de la espera.
                            last_resume = Clock::now();
                        }
                        return !cancel.load();
                    };
                    // Por defecto Ti recibe Bi. --threads activa una variante con P
                    // trabajadores para medir escalabilidad con la MISMA partición.
                    // tile_index es la variable privada que identifica el trabajo del hilo.
                    // Modo estático: tile_index = i. Modo dinámico: el valor anterior de
                    // next_tile se asigna atómicamente a un único consumidor.
                    std::size_t tile_index = worker_limit == 0 ? i : next_tile.fetch_add(1);
                    // Cada iteración calcula una región completa, salvo cancelación.
                    // El rango válido es [0,M), conservado en count.
                    while (tile_index < count && !cancel.load()) {
                        // Los píxeles de tiles.at(tile_index) son exclusivos de este trabajador.
                        // Muestras y rebotes se ejecutan secuencialmente dentro de render_tile.
                        const bool done = render_tile(scene, camera, image, tiles.at(tile_index), keep_running);
                        // Publica el resultado después de las escrituras; la región parcial no se
                        // notifica como completa. Los atómicos usan consistencia secuencial.
                        complete[tile_index].store(done);
                        // Termina por cancelación o por cumplir la asignación estática de un tile.
                        if (!done || worker_limit == 0) break;
                        // En el grupo limitado, solicita otra región sin duplicar índices.
                        // El tiempo de cada tile puede variar, por lo que el reparto es dinámico.
                        tile_index = next_tile.fetch_add(1); // Un único dueño por tile.
                    }
                } catch (...) {
                    // Registra la excepción en una casilla propia y cancela el grupo.
                    errors[i] = std::current_exception();
                    stop();
                }
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    // Cuenta la terminación del hilo incluso si hubo error dentro de su tarea.
                    ++finished;
                }
                wake.notify_all();
            });
        }
        // Supervisión en el principal: consulta eventos y consume avisos de tiles.
        while (!cancel.load()) {
            if (hooks.keep_running && !hooks.keep_running()) {
                stop();
                break;
            }
            // Este i recorre tiles terminados, no trabajadores. notified evita repetir
            // un callback; solo el coordinador modifica ese vector.
            for (std::size_t i = 0; i < count; ++i) if (!notified[i] && complete[i].load()) {
                notified[i] = true;
                if (hooks.tile_completed) hooks.tile_completed(tiles.at(i));
            }
            // Todos los trabajadores finalizaron; ya no se necesita esperar avance.
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
    // Tras join no existen escritores activos en las casillas de excepción.
    for (const auto& error : errors) if (error) std::rethrow_exception(error);
    for (std::size_t i = 0; i < count; ++i) if (complete[i].load()) {
        ++stats.completed;
        if (!notified[i] && hooks.tile_completed) hooks.tile_completed(tiles.at(i));
    }
    stats.workers = workers.size();
    // Exporta el total de esperas simuladas como propiedad de esta ejecución.
    stats.blocks = blocks.load();
    stats.loop_time = Clock::now() - start;
    // Tiempo de pared: incluye creación, coordinación, bloqueos y join.
    stats.render_time = stats.loop_time;
    return stats;
}
} // namespace rt
