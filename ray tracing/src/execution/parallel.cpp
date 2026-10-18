/**
 * @file parallel.cpp
 * Ejecución de tiles mediante un grupo de trabajadores con afinidad de CPU.
 * CMP y SMT comparten la distribución dinámica; difieren en la selección de CPUs.
 *
 * Variable de descomposición: índice de tile en [0, tiles.size()).
 * next es el contador atómico que asigna índices; index es la copia privada
 * del índice reclamado por un trabajador. i identifica al trabajador, no al tile.
 * fetch_add(1) devuelve el valor anterior de next de forma indivisible: dos
 * trabajadores no reciben el mismo índice. El contador puede superar el total
 * al agotarse el trabajo, pero el límite se comprueba antes de consultar TileGrid.
 *
 * Cada tile tiene un único propietario y contiene píxeles exclusivos. Escena,
 * cámara y grilla son de solo lectura; image comparte almacenamiento, pero sus
 * regiones de escritura son disjuntas. Los bucles de píxeles, muestras, esferas
 * y rebotes permanecen secuenciales dentro de cada trabajador. No hay una
 * reducción paralela de colores ni un hilo por rayo.
 *
 * complete publica la finalización; finished cuenta trabajadores terminados.
 * Los atómicos usan por defecto consistencia secuencial. La publicación de
 * complete sucede después de escribir la región; join sincroniza todas las
 * escrituras antes de devolver la imagen. El principal procesa los callbacks.
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
namespace {
// Motor común de SMT/CMP. Solo cambia la asignación de CPUs a los trabajadores.
// No hay cuantums fine ni bloqueos artificiales coarse en estos modelos.
RenderStats render_pool(const Scene& scene, const Camera& camera, Image& image,
                        const TileGrid& tiles, const std::vector<int>& cpus,
                        const RenderHooks& hooks) {
    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();
    // next: siguiente índice global de tile aún no reclamado.
    // finished: número de trabajadores que ya salieron de su función.
    // Ambos son compartidos; las operaciones atómicas evitan incrementos perdidos.
    std::atomic<std::size_t> next{0}, finished{0};
    // Señal compartida de cancelación, consultada entre píxeles y tiles.
    std::atomic<bool> cancel{false};
    // Una bandera por tile publica que su región está completamente calculada.
    std::vector<std::atomic<bool>> complete(tiles.size());
    // La inicialización explícita precede a la creación de cualquier hilo.
    for (auto& flag : complete) flag.store(false);
    // Registro privado del coordinador: impide notificar dos veces un tile.
    // Los trabajadores no acceden a este vector, evitando carreras sobre sus bits.
    std::vector<bool> notified(tiles.size(), false); // Solo el hilo principal.
    // Cada trabajador i dispone de una casilla exclusiva para su excepción.
    std::vector<std::exception_ptr> errors(cpus.size());
    std::vector<std::thread> workers;
    // Reserva P posiciones para los objetos std::thread, sin iniciar trabajo aún.
    workers.reserve(cpus.size());
    // El mutex serializa cambios del predicado y el comienzo de las esperas.
    // No protege image: la exclusividad se obtiene mediante índices de tile.
    std::mutex mutex;
    // Permite suspender al coordinador hasta una finalización o cancelación.
    std::condition_variable wake;
    // Captura por referencia el estado compartido para publicar cancelación.
    // El bloqueo coordina el predicado con wait_for y evita notificaciones perdidas.
    const auto stop = [&] {
        { std::lock_guard<std::mutex> lock(mutex); cancel.store(true); }
        wake.notify_all();
    };
    // Espera el fin de cada hilo; después de join sus escrituras son observables
    // y sus referencias a escena, cámara y buffer ya no siguen en uso.
    const auto join = [&] { for (auto& worker : workers) worker.join(); };
    try {
        if (hooks.keep_running && !hooks.keep_running()) stop();
        // Bucle de creación de P trabajadores. i es identidad de hilo, no trabajo.
        // cpus[i] es el destino de afinidad; un trabajador puede calcular varios tiles.
        for (std::size_t i = 0; i < cpus.size() && !cancel.load(); ++i) {
            // Construye e inicia un hilo. [&, i] comparte referencias al contexto y
            // copia i, conservando una identidad estable aunque avance el bucle exterior.
            workers.emplace_back([&, i] {
                try {
                    // Aplica afinidad desde el propio trabajador antes de reclamar tiles.
                    pin_current_thread(cpus[i]); // Si falla, cancelar y propagar tras join.
                    // PARALELIZACIÓN POR TILES: cada fetch_add entrega un dueño
                    // distinto. Los píxeles del tile se calculan secuencialmente.
                    // Bucle consumidor de tareas: cada iteración reclama un tile completo.
                    // La cantidad de iteraciones por hilo depende de la carga y planificación.
                    while (!cancel.load()) {
                        // Punto de distribución paralela: incremento atómico de la variable next.
                        // El valor devuelto se almacena en index, que pertenece solo a este hilo.
                        // Ejemplo: dos solicitudes simultáneas reciben 0 y 1, en algún orden;
                        // nunca reciben ambas 0. No se protege el render completo con un mutex.
                        const auto index = next.fetch_add(1);
                        // Índice fuera de [0,M): no queda trabajo; termina este consumidor.
                        // Reclamar M o más no accede al buffer ni a un rectángulo inexistente.
                        if (index >= tiles.size()) break;
                        // index selecciona un rectángulo disjunto. render_tile recorre sus píxeles
                        // en este mismo hilo; el callback consulta cancel sin atender eventos SDL.
                        const bool done = render_tile(scene, camera, image, tiles.at(index),
                                                      [&] { return !cancel.load(); });
                        // Publicar después de escribir todos los píxeles del tile.
                        // Publicación posterior a las escrituras de imagen. done solo es verdadero
                        // si se completó todo el tile; false conserva la condición de imagen parcial.
                        complete[index].store(done);
                        // Una cancelación dentro del tile interrumpe el consumo de nuevas tareas.
                        if (!done) break;
                    }
                } catch (...) {
                    // Almacena el fallo en la casilla privada de i; stop cancela a los demás.
                    // La excepción se relanza desde el coordinador después de todos los join.
                    errors[i] = std::current_exception();
                    stop();
                }
                // Registra una salida, incluso si el trabajador capturó una excepción.
                { std::lock_guard<std::mutex> lock(mutex); ++finished; }
                wake.notify_all();
            });
        }
        // El coordinador supervisa avance y eventos mientras trabajan los hilos.
        while (!cancel.load()) {
            if (hooks.keep_running && !hooks.keep_running()) { stop(); break; }
            // Aquí i sí es índice de tile. La lectura de complete sincroniza con
            // la publicación del trabajador; la notificación pertenece al coordinador.
            for (std::size_t i = 0; i < tiles.size(); ++i) if (!notified[i] && complete[i].load()) {
                notified[i] = true;
                if (hooks.tile_completed) hooks.tile_completed(tiles.at(i));
            }
            // La cantidad de salidas iguala los hilos creados: termina la supervisión.
            if (finished.load() == workers.size()) break;
            // wait_for libera el mutex mientras espera y lo recupera antes de retornar.
            std::unique_lock<std::mutex> lock(mutex);
            // Atender eventos aunque no termine ningún tile durante este intervalo.
            // Espera acotada a 5 ms: mantiene respuesta a eventos aunque ningún tile
            // termine. El predicado se comprueba al despertar, también ante despertares espurios.
            wake.wait_for(lock, std::chrono::milliseconds(5), [&] {
                return cancel.load() || finished.load() == workers.size();
            });
        }
    // Un fallo del coordinador o al crear hilos cancela y reúne los ya creados
    // antes de propagar la excepción, evitando destruir hilos aún activos.
    } catch (...) {
        stop();
        join();
        throw;
    }
    join(); // No presentar ni guardar el buffer antes de que termine toda escritura.
    // Tras join se leen las excepciones sin acceso concurrente a sus casillas.
    for (const auto& error : errors) if (error) std::rethrow_exception(error);
    RenderStats stats;
    // Recuento final de tiles completos; recupera notificaciones que la última
    // iteración de supervisión no hubiera procesado.
    for (std::size_t i = 0; i < tiles.size(); ++i) if (complete[i].load()) {
        ++stats.completed;
        if (!notified[i] && hooks.tile_completed) hooks.tile_completed(tiles.at(i));
    }
    // P real: cantidad de hilos creados, excluyendo al coordinador.
    stats.workers = workers.size();
    // Tiempo de pared del grupo: creación, afinidad, cálculo, coordinación y join.
    stats.loop_time = Clock::now() - start;
    stats.render_time = stats.loop_time;
    return stats;
}
} // namespace
// Selecciona una CPU lógica por núcleo físico; después reutiliza render_pool.
// La selección de CPUs también queda incluida en el tiempo de este modelo.
RenderStats render_cmp(const Scene& scene, const Camera& camera, Image& image,
                       const TileGrid& tiles, const CpuTopology& topology,
                       std::size_t workers, const RenderHooks& hooks) {
    const auto start = std::chrono::steady_clock::now();
    auto stats = render_pool(scene, camera, image, tiles, cmp_cpus(topology, tiles.size(), workers), hooks);
    stats.loop_time = stats.render_time = std::chrono::steady_clock::now() - start;
    return stats;
}
// Selecciona CPUs lógicas y, si corresponde, repite destinos por sobresuscripción.
// El reparto atómico de tiles permanece idéntico al modelo CMP.
RenderStats render_smt(const Scene& scene, const Camera& camera, Image& image,
                       const TileGrid& tiles, const CpuTopology& topology,
                       std::size_t workers, const RenderHooks& hooks) {
    const auto start = std::chrono::steady_clock::now();
    auto stats = render_pool(scene, camera, image, tiles, smt_cpus(topology, tiles.size(), workers), hooks);
    stats.loop_time = stats.render_time = std::chrono::steady_clock::now() - start;
    return stats;
}
} // namespace rt
