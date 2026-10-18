#pragma once
#include "interferogram/interferogram.hpp"
#include <condition_variable>
#include <mutex>
#include <exception>
#include <thread>
#include <atomic>

namespace interferogram {
// Pool de hilos persistentes que procesan tiles completos. Los hilos se crean una
// vez y se reutilizan en cada compute(), así la medición no incluye su creación.
// También sirve de base a Smt, que le pasa CPUs fijas y reparto dinámico.
class CoarseGrained {
public:
    // cpus vacío: sin afinidad. dynamicTiles: los tiles se toman de una cola atómica
    // en lugar de repartirse en rangos fijos por trabajador.
    CoarseGrained(int width, int height, int tileWidth, int tileHeight, int threads,
                  const std::vector<int>& cpus = {}, bool dynamicTiles = false);
    ~CoarseGrained();
    CoarseGrained(const CoarseGrained&) = delete;
    CoarseGrained& operator=(const CoarseGrained&) = delete;
    void compute(const std::vector<Complex>& a, const std::vector<Complex>& b,
                 std::vector<Complex>& result);
    std::size_t workers() const { return threads_.size(); }
    std::size_t tasks() const { return tiles_.size(); }
private:
    void work(std::size_t begin, std::size_t end, int cpu);
    void stop();
    int width_;
    bool dynamicTiles_;
    std::atomic<std::size_t> nextTile_{0};
    std::size_t samples_;
    std::vector<Tile> tiles_;
    std::vector<std::thread> threads_;
    std::mutex mutex_;
    std::condition_variable ready_, done_;
    std::exception_ptr startupError_;
    bool stopping_ = false;
    // generation_ cambia en cada compute(); remaining_ cuenta trabajadores sin terminar.
    std::size_t generation_ = 0, remaining_ = 0, started_ = 0;
    // Entradas y salida del cálculo en curso; solo son válidas durante compute().
    const std::vector<Complex>* a_ = nullptr;
    const std::vector<Complex>* b_ = nullptr;
    std::vector<Complex>* result_ = nullptr;
};
} // namespace interferogram
