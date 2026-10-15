#ifndef DL_UPDATE_POOL_H
#define DL_UPDATE_POOL_H
#include <chrono>
#include <algorithm>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>
#include <stdexcept>

// Un dueño por coeficiente. El coordinador calcula todos los gradientes con
// los pesos anteriores y espera la barrera antes de la siguiente muestra SGD.
class UpdatePool {
public:
    struct Segment { double* weights; const double* gradients; size_t size; };
private:
    std::mutex mutex_;
    std::condition_variable ready_, done_;
    std::vector<std::thread> threads_;
    std::vector<Segment> segments_;
    size_t generation_ = 0, remaining_ = 0, size_ = 0;
    bool stop_ = false;
    double rate_ = 0;
    std::chrono::milliseconds interval_, duration_;
    std::uint64_t blocks_ = 0;
    void work(size_t id, size_t workers) {
        size_t seen = 0;
        std::unique_lock<std::mutex> lock(mutex_);
        // El reloj avanza entre muestras: una actualización pequeña también
        // participa del bloqueo periódico durante un entrenamiento largo.
        auto resumed = std::chrono::steady_clock::now();
        while (true) {
            ready_.wait(lock, [&] { return stop_ || generation_ != seen; });
            if (stop_) return;
            seen = generation_;
            const size_t base = size_ / workers, extra = size_ % workers;
            const size_t begin = id * base + std::min(id, extra);
            const size_t end = begin + base + (id < extra);
            lock.unlock();
            std::uint64_t blocks = 0;
            size_t offset = 0;
            for (const auto& segment : segments_) {
                const size_t first = begin > offset ? std::min(segment.size, begin - offset) : 0;
                const size_t last = end > offset ? std::min(segment.size, end - offset) : 0;
                for (size_t i = first; i < last; ++i) {
                    if ((i - first) % 256 == 0 && interval_.count() > 0 &&
                        std::chrono::steady_clock::now() - resumed >= interval_) {
                        lock.lock();
                        if (ready_.wait_for(lock, duration_, [&] { return stop_; })) return;
                        lock.unlock();
                        ++blocks;
                        resumed = std::chrono::steady_clock::now();
                    }
                    segment.weights[i] -= rate_ * segment.gradients[i];
                }
                offset += segment.size;
            }
            lock.lock();
            blocks_ += blocks;
            if (--remaining_ == 0) done_.notify_one();
        }
    }
public:
    UpdatePool(size_t workers, int interval_ms, int duration_ms)
        : interval_(interval_ms), duration_(duration_ms) {
        if (!workers || interval_ms < 0 || duration_ms < 0) throw std::invalid_argument("Configuración de pool inválida");
        try {
            for (size_t id = 0; id < workers; ++id) threads_.emplace_back(&UpdatePool::work, this, id, workers);
        } catch (...) { close(); throw; }
    }
    ~UpdatePool() { close(); }
    void apply(std::vector<Segment> segments, double rate) {
        std::unique_lock<std::mutex> lock(mutex_);
        segments_ = std::move(segments); rate_ = rate; size_ = 0;
        for (const auto& segment : segments_) size_ += segment.size;
        remaining_ = threads_.size(); ++generation_;
        ready_.notify_all();
        done_.wait(lock, [&] { return remaining_ == 0; });
    }
    void close() {
        { std::lock_guard<std::mutex> lock(mutex_); stop_ = true; }
        ready_.notify_all();
        for (auto& thread : threads_) if (thread.joinable()) thread.join();
    }
    std::uint64_t blocks() const { return blocks_; } // Consultar después de close().
};
#endif
