#include "interferogram/models/coarse_grained.hpp"
#include "interferogram/kernel.hpp"
#include <algorithm>
#include <stdexcept>

namespace interferogram {
CoarseGrained::CoarseGrained(int width, int height, int tileWidth, int tileHeight, int threads)
    : width_(width), samples_(0), tiles_(makeTiles(width, height, tileWidth, tileHeight)) {
    if (threads <= 0) throw std::invalid_argument("La cantidad de hilos debe ser positiva.");
    samples_ = static_cast<std::size_t>(width) * height;
    threads_.reserve(threads);
    try {
        const std::size_t base = tiles_.size() / threads;
        const std::size_t extra = tiles_.size() % threads;
        for (int worker = 0; worker < threads; ++worker) {
            const std::size_t begin = worker * base + std::min<std::size_t>(worker, extra);
            const std::size_t end = begin + base + (static_cast<std::size_t>(worker) < extra);
            threads_.emplace_back([this, begin, end] { work(begin, end); });
        }
        // Esperar el arranque para excluirlo de la medición posterior.
        std::unique_lock<std::mutex> lock(mutex_);
        done_.wait(lock, [this] { return started_ == threads_.size(); });
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

void CoarseGrained::work(std::size_t begin, std::size_t end) {
    std::size_t seen = 0;
    std::unique_lock<std::mutex> lock(mutex_);
    ++started_;
    done_.notify_one();
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
        for (std::size_t t = begin; t < end; ++t) {
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
    remaining_ = threads_.size();
    ++generation_;
    ready_.notify_all();
    done_.wait(lock, [this] { return remaining_ == 0; });
    a_ = nullptr;
    b_ = nullptr;
    result_ = nullptr;
}
} // namespace interferogram
