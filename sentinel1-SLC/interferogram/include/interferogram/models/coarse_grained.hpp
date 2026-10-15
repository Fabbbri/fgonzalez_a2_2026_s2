#pragma once
#include "interferogram/interferogram.hpp"
#include <condition_variable>
#include <mutex>
#include <exception>
#include <thread>
#include <atomic>

namespace interferogram {
// Bloques completos
class CoarseGrained {
public:
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
    std::size_t generation_ = 0, remaining_ = 0, started_ = 0;
    const std::vector<Complex>* a_ = nullptr;
    const std::vector<Complex>* b_ = nullptr;
    std::vector<Complex>* result_ = nullptr;
};
} // namespace interferogram
