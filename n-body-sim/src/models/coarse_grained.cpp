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
    using Clock = std::chrono::steady_clock;
    const std::chrono::milliseconds block_interval_, block_duration_;
    std::vector<std::thread> threads_;
    std::mutex mutex_;
    std::condition_variable work_ready_, phase_done_;
    bool stopping_ = false;
    std::size_t generation_ = 0, remaining_ = 0;
    const std::vector<Body>* bodies_ = nullptr;
    std::vector<Vec2>* accelerations_ = nullptr;
    double gravity_ = 0, epsilon_ = 0;
    SchedulingStats stats_;

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        work_ready_.notify_all();
        for (auto& thread : threads_) thread.join();
    }

    void run(std::size_t worker, std::size_t workers) {
        std::size_t seen = 0;
        std::unique_lock<std::mutex> lock(mutex_);
        for (;;) {
            work_ready_.wait(lock, [&] { return stopping_ || generation_ != seen; });
            if (stopping_) return;
            seen = generation_;
            const auto& bodies = *bodies_;
            auto& accelerations = *accelerations_;
            const double gravity = gravity_, epsilon = epsilon_;
            // Partición equilibrada sin multiplicar N por el índice del hilo.
            const std::size_t base = bodies.size() / workers;
            const std::size_t extra = bodies.size() % workers;
            const std::size_t begin = worker * base + (worker < extra ? worker : extra);
            const std::size_t end = begin + base + (worker < extra ? 1 : 0);
            lock.unlock();
            auto last_resume = Clock::now();
            std::uint64_t blocks = 0;
            for (std::size_t i = begin; i < end; ++i) {
                if (block_interval_.count() > 0 && Clock::now() - last_resume >= block_interval_) {
                    ++blocks;
                    lock.lock();
                    // wait_for libera el mutex: otros trabajadores pueden avanzar.
                    // El índice i y las salidas ya calculadas conservan el progreso.
                    if (work_ready_.wait_for(lock, block_duration_, [&] { return stopping_; })) return;
                    lock.unlock();
                    last_resume = Clock::now();
                }
                accelerations[i] = acceleration_for_body(bodies, i, gravity, epsilon);
            }
            lock.lock();
            stats_.blocks += blocks;
            if (--remaining_ == 0) phase_done_.notify_one();
            // Esperar otra generación bloquea el hilo, sin espera activa.
        }
    }
public:
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
        threads_.reserve(workers);
        try {
            for (std::size_t i = 0; i < workers; ++i) {
                threads_.emplace_back([this, i, workers] { run(i, workers); });
#if defined(__linux__)
                if (!cpus.empty()) {
                    if (cpus[i] < 0 || cpus[i] >= CPU_SETSIZE) {
                        throw std::invalid_argument("CPU fuera del rango de afinidad.");
                    }
                    cpu_set_t mask;
                    CPU_ZERO(&mask);
                    CPU_SET(cpus[i], &mask);
                    const int error = pthread_setaffinity_np(threads_.back().native_handle(),
                                                            sizeof(mask), &mask);
                    if (error != 0) throw std::system_error(error, std::generic_category(), "Afinidad de trabajador");
                    // Confirmar la máscara efectiva antes de publicar cualquier trabajo.
                    CPU_ZERO(&mask);
                    const int read_error = pthread_getaffinity_np(threads_.back().native_handle(),
                                                                 sizeof(mask), &mask);
                    if (read_error != 0) throw std::system_error(read_error, std::generic_category(), "Leer afinidad de trabajador");
                    if (CPU_COUNT(&mask) != 1 || !CPU_ISSET(cpus[i], &mask)) {
                        throw std::runtime_error("No se pudo confirmar la afinidad del trabajador.");
                    }
                }
#endif
            }
        } catch (...) {
            stop();  // Un fallo de creación tampoco deja hilos sin unir.
            throw;
        }
        stats_.tasks = workers;
    }
    ~CoarseGrainedModel() override { stop(); }
    const char* name() const override { return "coarse-grained"; }
    std::size_t workers() const override { return threads_.size(); }
    SchedulingStats scheduling_stats() const override { return stats_; }

    void compute_accelerations(const std::vector<Body>& bodies,
                               std::vector<Vec2>& accelerations,
                               double gravity, double epsilon) override {
        accelerations.resize(bodies.size());
        std::unique_lock<std::mutex> lock(mutex_);
        bodies_ = &bodies;
        accelerations_ = &accelerations;
        gravity_ = gravity;
        epsilon_ = epsilon;
        remaining_ = threads_.size();
        ++generation_;
        work_ready_.notify_all();
        phase_done_.wait(lock, [&] { return remaining_ == 0; });
        ++stats_.rounds;  // Fases completas de sincronización, no cambios de contexto.
        stats_.dispatches += threads_.size();
        bodies_ = nullptr;
        accelerations_ = nullptr;
    }
};
}  // namespace

std::unique_ptr<ExecutionModel> make_coarse_grained_model(
    std::size_t workers, const std::vector<int>& cpus,
    int block_interval_ms, int block_duration_ms) {
    return std::make_unique<CoarseGrainedModel>(workers, cpus, block_interval_ms, block_duration_ms);
}
}  // namespace nbody
