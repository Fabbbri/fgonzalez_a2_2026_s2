#include "interferogram/models/fine_grained.hpp"
#include "interferogram/kernel.hpp"
#include <stdexcept>

namespace interferogram {
FineGrained::FineGrained(int width, int height, int tileWidth, int tileHeight, int quantum)
    : width_(width), quantum_(quantum) {
    if (quantum <= 0)
        throw std::invalid_argument("El quantum debe ser positivo.");
    for (const auto& tile : makeTiles(width, height, tileWidth, tileHeight))
        tasks_.push_back({tile, tile.xBegin, tile.yBegin});
    reset();
}

void FineGrained::reset() {
    for (auto& task : tasks_) {
        task.x = task.tile.xBegin;
        task.y = task.tile.yBegin;
    }
    pending_ = tasks_.size();
    rounds_ = 0;
    dispatches_ = 0;
}

void FineGrained::runRound(const std::vector<Complex>& a, const std::vector<Complex>& b,
                           std::vector<Complex>& result) {
    if (finished()) return;
    ++rounds_;
    // Round-robin: ceder por quantum, sin bloqueos.
    for (auto& task : tasks_) {
        if (task.y == task.tile.yEnd) continue;
        ++dispatches_;
        for (int processed = 0; processed < quantum_ && task.y < task.tile.yEnd; ++processed) {
            const std::size_t i = static_cast<std::size_t>(task.y) * width_ + task.x;
            result[i] = conjugateProduct(a[i], b[i]);
            if (++task.x == task.tile.xEnd) {
                task.x = task.tile.xBegin;
                ++task.y;
            }
        }
        if (task.y == task.tile.yEnd) --pending_;
    }
}
} // namespace interferogram
