#include "interferogram/models/coarse_grained.hpp"
#include "interferogram/kernel.hpp"
#include <algorithm>
#include <stdexcept>
#include <system_error>
#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif

namespace interferogram {
CoarseGrained::CoarseGrained(int width, int height, int tileWidth, int tileHeight, int threads,
                             const std::vector<int>& cpus, bool dynamicTiles)
    : width_(width), dynamicTiles_(dynamicTiles), samples_(0), tiles_(makeTiles(width, height, tileWidth, tileHeight)) {
    if (threads <= 0) throw std::invalid_argument("La cantidad de hilos debe ser positiva.");
    if (!cpus.empty() && cpus.size() != static_cast<std::size_t>(threads))
        throw std::invalid_argument("Una CPU por trabajador requerida.");
    samples_ = static_cast<std::size_t>(width) * height;
    threads_.reserve(threads);
    try {
        const std::size_t base = tiles_.size() / threads;
        const std::size_t extra = tiles_.size() % threads;
        for (int worker = 0; worker < threads; ++worker) {
            const std::size_t begin = worker * base + std::min<std::size_t>(worker, extra);
            const std::size_t end = begin + base + (static_cast<std::size_t>(worker) < extra);
            const int cpu = cpus.empty() ? -1 : cpus[worker];
            threads_.emplace_back([this, begin, end, cpu] { work(begin, end, cpu); });
        }
        // Esperar el arranque para excluirlo de la medición posterior.
        std::unique_lock<std::mutex> lock(mutex_);
        done_.wait(lock, [this] { return started_ == threads_.size(); });
        if (startupError_) std::rethrow_exception(startupError_);
    } catch (...) {
        stop();
        throw;
    }
}

CoarseGrained::~CoarseGrained() { 
    stop(); }

void CoarseGrained::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    ready_.notify_all();
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void CoarseGrained::work(std::size_t begin, std::size_t end, int cpu) {
    std::exception_ptr error;
    try {
        if (cpu >= 0) {
#ifdef __linux__
            if (cpu >= CPU_SETSIZE) throw std::runtime_error("CPU fuera de rango");
            cpu_set_t mask;
            CPU_ZERO(&mask);
            CPU_SET(cpu, &mask);
            const int status = pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask);
            if (status) throw std::system_error(status, std::generic_category(), "Afinidad SMT");
            cpu_set_t actual;
            CPU_ZERO(&actual);
            const int readStatus = pthread_getaffinity_np(pthread_self(), sizeof(actual), &actual);
            if (readStatus) throw std::system_error(readStatus, std::generic_category(), "Verificar afinidad SMT");
            if (CPU_COUNT(&actual) != 1 || !CPU_ISSET(cpu, &actual))
                throw std::runtime_error("Afinidad SMT distinta de la solicitada");
#else
            throw std::runtime_error("Afinidad SMT requiere Linux");
#endif
        }
    } catch (...) { error = std::current_exception(); }

    std::size_t seen = 0;
    std::unique_lock<std::mutex> lock(mutex_);
    if (error) startupError_ = error;
    ++started_;
    done_.notify_one();
    if (error) return;
    for (;;) {
        ready_.wait(lock, [this, &seen] { return stopping_ || generation_ != seen; });
        if (stopping_) {
            return;
        }
        seen = generation_;
        const auto& a = *a_;
        const auto& b = *b_;
        auto& result = *result_;
        lock.unlock();
        // Regiones disjuntas: no se bloquea por píxel ni se comparte la escritura.
        // fetch_add da un único dueño a cada tile. La publicación de entradas y
        // la finalización se sincronizan con mutex_; el índice solo asigna trabajo.
        for (std::size_t t = dynamicTiles_ ? nextTile_.fetch_add(1, std::memory_order_relaxed) : begin;
             t < (dynamicTiles_ ? tiles_.size() : end);
             t = dynamicTiles_ ? nextTile_.fetch_add(1, std::memory_order_relaxed) : t + 1) {
            const auto& tile = tiles_[t];
            for (int y = tile.yBegin; y < tile.yEnd; ++y)
                for (int x = tile.xBegin; x < tile.xEnd; ++x) {
                    const std::size_t i = static_cast<std::size_t>(y) * width_ + x;
                    result[i] = conjugateProduct(a[i], b[i]);
                }
        }
        lock.lock();
        if (--remaining_ == 0) {
            done_.notify_one();
        }
    }
}

void CoarseGrained::compute(const std::vector<Complex>& a, const std::vector<Complex>& b,
                            std::vector<Complex>& result) {
    validateInterferogramInputs(a, b, result);
    if (a.size() != samples_) {
        throw std::invalid_argument("La ventana no coincide con el pool.");
    }
    std::unique_lock<std::mutex> lock(mutex_);
    a_ = &a;
    b_ = &b;
    result_ = &result;
    nextTile_.store(0, std::memory_order_relaxed);
    remaining_ = threads_.size();
    ++generation_;
    ready_.notify_all();
    done_.wait(lock, [this] { return remaining_ == 0; });
    a_ = nullptr;
    b_ = nullptr;
    result_ = nullptr;
}
} // namespace interferogram
