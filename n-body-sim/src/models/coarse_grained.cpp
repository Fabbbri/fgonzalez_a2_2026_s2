#include "nbody.hpp"
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace nbody {
namespace {
// Bloques completos, sin cesiones por quantum. El bloqueo explícito ocurre
// entre fases mediante variables de condición; el SO decide la planificación.
class CoarseGrainedModel final : public ExecutionModel {
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
            for (std::size_t i = begin; i < end; ++i) {
                accelerations[i] = acceleration_for_body(bodies, i, gravity, epsilon);
            }
            lock.lock();
            if (--remaining_ == 0) phase_done_.notify_one();
            // Esperar otra generación bloquea el hilo, sin espera activa.
        }
    }
public:
    explicit CoarseGrainedModel(std::size_t workers) {
        if (workers == 0) throw std::invalid_argument("La cantidad de hilos debe ser positiva.");
        threads_.reserve(workers);
        try {
            for (std::size_t i = 0; i < workers; ++i) {
                threads_.emplace_back([this, i, workers] { run(i, workers); });
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

std::unique_ptr<ExecutionModel> make_coarse_grained_model(std::size_t workers) {
    return std::make_unique<CoarseGrainedModel>(workers);
}
}  // namespace nbody
