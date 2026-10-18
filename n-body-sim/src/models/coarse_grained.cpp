/**
 * @file coarse_grained.cpp
 * Grupo persistente de trabajadores con partición estática de cuerpos.
 *
 * Variable paralelizada: i, índice del cuerpo receptor de aceleración.
 * El trabajador worker recibe un intervalo disjunto [begin,end) de i.
 * Cada i recorre las fuentes j en orden dentro de acceleration_for_body.
 * No se distribuyen pasos temporales ni galaxias: cada cuerpo recibe
 * la atracción de todos los demás cuerpos de ambas galaxias.
 *
 * El coordinador publica una fase mediante generation_ bajo mutex_. Los
 * trabajadores adquieren punteros y parámetros, liberan el mutex y calculan
 * salidas disjuntas. bodies_ permanece de solo lectura hasta que remaining_
 * llega a cero. phase_done_ actúa como barrera de finalización de la fase.
 * Los hilos se reutilizan entre fases; join corresponde a la destrucción del grupo.
 *
 * Los bloqueos temporizados conservan i y las salidas ya calculadas. El sistema
 * operativo decide la planificación efectiva y puede desplanificar fuera de esas
 * esperas. SMT y CMP reutilizan este grupo con bloqueos desactivados y afinidad.
 */
#include "nbody.hpp"
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <system_error>
#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

namespace nbody {
namespace {
// Bloques persistentes: el reloj provoca esperas individuales entre cuerpos,
// conservando el progreso. El SO decide qué hilo listo ejecuta durante la pausa.
class CoarseGrainedModel final : public ExecutionModel {
    // Reloj monotónico para las esperas; no depende de cambios de la hora del sistema.
    using Clock = std::chrono::steady_clock;
    // Intervalo entre bloqueos y duración solicitada de espera, fijos para el grupo.
    const std::chrono::milliseconds block_interval_, block_duration_;
    // Objetos de los P trabajadores persistentes, creados una vez y reutilizados por fase.
    std::vector<std::thread> threads_;
    // Protege publicación de fase, predicados y estadísticas compartidas;
    // no se mantiene tomado durante el cálculo de fuerzas.
    std::mutex mutex_;
    // work_ready_ comunica nueva generación o parada; phase_done_ comunica fin de fase.
    std::condition_variable work_ready_, phase_done_;
    // Predicado de terminación, leído y modificado con mutex_ adquirido.
    bool stopping_ = false;
    // generation_ identifica la fase publicada; remaining_ cuenta trabajadores no terminados.
    std::size_t generation_ = 0, remaining_ = 0;
    // Instantánea de entrada prestada por el coordinador; válida hasta terminar la fase.
    const std::vector<Body>* bodies_ = nullptr;
    // Buffer de salidas ya dimensionado: cada hilo escribe solo su intervalo de índices.
    std::vector<Vec2>* accelerations_ = nullptr;
    // Parámetros físicos comunes publicados junto a los punteros bajo el mismo mutex.
    double gravity_ = 0, epsilon_ = 0;
    // Acumuladores de ejecución: rondas, despachos y esperas de todas las fases.
    SchedulingStats stats_;

    // Publica terminación, despierta trabajadores y los reúne antes de destruir
    // los datos compartidos. También cubre construcción parcial del grupo.
    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            // El cambio bajo mutex evita que un trabajador pierda la señal antes de wait.
            stopping_ = true;
        }
        // Despierta las esperas para que observen stopping_ y finalicen el servicio.
        work_ready_.notify_all();
        // join sincroniza el final de cada hilo; los objetos compartidos siguen vivos hasta entonces.
        for (auto& thread : threads_) thread.join();
    }

    // Función del trabajador: worker es su identificador en [0,P), no índice de cuerpo.
    // workers = P permanece fijo; el intervalo de cuerpos se obtiene para cada fase.
    void run(std::size_t worker, std::size_t workers) {
        // Última generación atendida por este hilo; evita repetir la misma fase.
        std::size_t seen = 0;
        // El cerrojo se conserva como objeto y se libera al calcular o esperar.
        std::unique_lock<std::mutex> lock(mutex_);
        // Bucle de servicio persistente: procesa generaciones hasta recibir stopping_.
        for (;;) {
            // wait libera el mutex mientras bloquea y lo recupera antes de verificar el
            // predicado. Despertares espurios no autorizan repetir una generación.
            work_ready_.wait(lock, [&] { return stopping_ || generation_ != seen; });
            // La parada toma prioridad sobre una fase publicada; el retorno libera el cerrojo.
            if (stopping_) return;
            // Marca la fase recibida antes de soltar el mutex para comenzar el trabajo.
            seen = generation_;
            // Alias de lectura de la misma instantánea para todos los trabajadores.
            const auto& bodies = *bodies_;
            // Alias del buffer de salida común; la propiedad de elementos es disjunta.
            auto& accelerations = *accelerations_;
            // Copias privadas de parámetros físicos mientras se conserva la publicación bloqueada.
            const double gravity = gravity_, epsilon = epsilon_;
            // Partición equilibrada sin multiplicar N por el índice del hilo.
            // q = floor(N/P): cantidad mínima de cuerpos por trabajador.
            const std::size_t base = bodies.size() / workers;
            // r = N%P: los primeros r trabajadores reciben un cuerpo adicional.
            const std::size_t extra = bodies.size() % workers;
            // Inicio B_w = w*q+min(w,r). Su unidad es cuerpo receptor.
            // El intervalo del trabajador anterior termina exactamente aquí.
            const std::size_t begin = worker * base + (worker < extra ? worker : extra);
            // Fin exclusivo E_w = B_w+q+(w<r). Los intervalos cubren [0,N)
            // sin superposición; si P>N algunos intervalos quedan vacíos.
            const std::size_t end = begin + base + (worker < extra ? 1 : 0);
            // Libera el mutex antes del costo O(N²/P), permitiendo cálculo simultáneo.
            lock.unlock();
            // Marca temporal privada para decidir bloqueos dentro de esta fase.
            auto last_resume = Clock::now();
            // Contador local de esperas; se incorpora a stats_ bajo mutex al finalizar.
            std::uint64_t blocks = 0;
            // Bucle paralelizado por partición de su dominio: i identifica el cuerpo
            // receptor de aceleración. Cada hilo ejecuta su subintervalo en orden.
            // La iteración i calcula una suma completa sobre fuentes j; no modifica bodies.
            for (std::size_t i = begin; i < end; ++i) {
                // Cero desactiva bloqueos. El tiempo de pared puede incluir desplanificación del SO.
                if (block_interval_.count() > 0 && Clock::now() - last_resume >= block_interval_) {
                    ++blocks;
                    // Recupera el mutex para verificar parada y entrar en la espera de condición.
                    lock.lock();
                    // wait_for libera el mutex: otros trabajadores pueden avanzar.
                    // El índice i y las salidas ya calculadas conservan el progreso.
                    // Una parada interrumpe la espera; un aviso de fase sin stopping_ no la completa.
                    // La suspensión conserva i: el cuerpo actual aún no se ha calculado.
                    if (work_ready_.wait_for(lock, block_duration_, [&] { return stopping_; })) return;
                    lock.unlock();
                    last_resume = Clock::now();
                }
                // Escritura exclusiva de la variable de salida asociada a i.
                // acceleration_for_body lee todas las posiciones, pero acumula en un Vec2 local.
                // No hay mutex por fuerza ni suma compartida entre trabajadores.
                accelerations[i] = acceleration_for_body(bodies, i, gravity, epsilon);
            }
            // Reingresa en la sección crítica para publicar término y acumular estadísticas.
            lock.lock();
            // La suma compartida se serializa; no requiere contador atómico adicional.
            stats_.blocks += blocks;
            // El último trabajador despierta al coordinador. Como cada finalización
            // adquiere el mismo mutex, la barrera publica también todas las salidas.
            if (--remaining_ == 0) phase_done_.notify_one();
            // Esperar otra generación bloquea el hilo, sin espera activa.
        }
    }
public:
    // Construye el grupo persistente y, si hay destinos, fija y verifica afinidad
    // antes de publicar trabajo físico. cpus vacío deja afinidad al SO.
    explicit CoarseGrainedModel(std::size_t workers, const std::vector<int>& cpus,
                               int block_interval_ms, int block_duration_ms)
        : block_interval_(block_interval_ms), block_duration_(block_duration_ms) {
        if (block_interval_ms < 0 || block_duration_ms < 0) {
            throw std::invalid_argument("Los tiempos de bloqueo no pueden ser negativos.");
        }
        if (workers == 0) throw std::invalid_argument("La cantidad de hilos debe ser positiva.");
        if (!cpus.empty() && cpus.size() != workers) {
            throw std::invalid_argument("Se requiere una CPU por trabajador.");
        }
#if !defined(__linux__)
        if (!cpus.empty()) throw std::invalid_argument("Afinidad explícita disponible solo en Linux.");
#endif
        // Reserva objetos de hilos; evita realocación durante su construcción.
        threads_.reserve(workers);
        try {
            // Este i identifica trabajadores y sus destinos cpus[i], no cuerpos.
            for (std::size_t i = 0; i < workers; ++i) {
                // La lambda copia i y P, y conserva this para acceder al estado que vive
                // hasta el destructor. El hilo comienza bloqueado a la espera de generación.
                threads_.emplace_back([this, i, workers] { run(i, workers); });
#if defined(__linux__)
                if (!cpus.empty()) {
                    if (cpus[i] < 0 || cpus[i] >= CPU_SETSIZE) {
                        throw std::invalid_argument("CPU fuera del rango de afinidad.");
                    }
                    // Máscara de afinidad fija de Linux; los destinos se validan contra CPU_SETSIZE.
                    cpu_set_t mask;
                    CPU_ZERO(&mask);
                    // Selecciona solo la CPU lógica de este trabajador.
                    CPU_SET(cpus[i], &mask);
                    // Aplica afinidad al hilo recién creado mediante su identificador nativo.
                    const int error = pthread_setaffinity_np(threads_.back().native_handle(),
                                                            sizeof(mask), &mask);
                    if (error != 0) throw std::system_error(error, std::generic_category(), "Afinidad de trabajador");
                    // Confirmar la máscara efectiva antes de publicar cualquier trabajo.
                    CPU_ZERO(&mask);
                    // Consulta la máscara efectiva para comprobar que la asignación fue aplicada.
                    const int read_error = pthread_getaffinity_np(threads_.back().native_handle(),
                                                                 sizeof(mask), &mask);
                    if (read_error != 0) throw std::system_error(read_error, std::generic_category(), "Leer afinidad de trabajador");
                    if (CPU_COUNT(&mask) != 1 || !CPU_ISSET(cpus[i], &mask)) {
                        throw std::runtime_error("No se pudo confirmar la afinidad del trabajador.");
                    }
                }
#endif
            }
        // Ante fallo de construcción o afinidad, stop reúne todos los hilos ya creados
        // antes de relanzar el error, evitando std::terminate al destruirlos activos.
        } catch (...) {
            stop();  // Un fallo de creación tampoco deja hilos sin unir.
            throw;
        }
        // En el grupo estático, las tareas registradas son P particiones persistentes.
        stats_.tasks = workers;
    }
    // La destrucción cierra el servicio de trabajadores mediante stop y join.
    ~CoarseGrainedModel() override { stop(); }
    // Etiqueta de estrategia para consola, CSV y selección experimental.
    const char* name() const override { return "coarse-grained"; }
    // P efectivo es el tamaño del grupo; el coordinador no se cuenta.
    std::size_t workers() const override { return threads_.size(); }
    // Copia de estadísticas consultada por el coordinador entre fases completas.
    SchedulingStats scheduling_stats() const override { return stats_; }

    // Publica una fase completa y bloquea al coordinador hasta que terminan los P hilos.
    // Las llamadas a este método son secuenciales desde el integrador.
    void compute_accelerations(const std::vector<Body>& bodies,
                               std::vector<Vec2>& accelerations,
                               double gravity, double epsilon) override {
        // Dimensiona antes de despertar a los hilos; durante la fase no cambia el almacenamiento.
        accelerations.resize(bodies.size());
        std::unique_lock<std::mutex> lock(mutex_);
        // Presta entrada y salida hasta que la barrera de remaining_ se satisfaga.
        bodies_ = &bodies;
        accelerations_ = &accelerations;
        gravity_ = gravity;
        epsilon_ = epsilon;
        // Incluso un trabajador con intervalo vacío participa una vez en el fin de fase.
        remaining_ = threads_.size();
        // Nueva identidad de fase: cada trabajador verá generation_ != seen al despertar.
        ++generation_;
        // Despierta a los trabajadores con una nueva identidad y argumentos de fase.
        work_ready_.notify_all();
        // Libera el mutex mientras espera; el retorno con remaining_==0 asegura
        // que ninguna tarea sigue leyendo positions ni escribiendo accelerations.
        phase_done_.wait(lock, [&] { return remaining_ == 0; });
        ++stats_.rounds;  // Fases completas de sincronización, no cambios de contexto.
        // Un despacho por trabajador en cada fase, separado de cambios de contexto reales.
        stats_.dispatches += threads_.size();
        // Retira los punteros prestados una vez que los hilos han terminado la fase.
        bodies_ = nullptr;
        accelerations_ = nullptr;
    }
};
}  // namespace

// Fábrica polimórfica con propiedad exclusiva del grupo y destrucción RAII.
std::unique_ptr<ExecutionModel> make_coarse_grained_model(
    std::size_t workers, const std::vector<int>& cpus,
    int block_interval_ms, int block_duration_ms) {
    return std::make_unique<CoarseGrainedModel>(workers, cpus, block_interval_ms, block_duration_ms);
}
}  // namespace nbody
