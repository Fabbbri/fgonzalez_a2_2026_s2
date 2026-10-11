#include "interferogram/interferogram.hpp"
#include "interferogram/kernel.hpp"

namespace interferogram {
void computeInterferogramTiled(const std::vector<Complex>& a,
    const std::vector<Complex>& b,std::vector<Complex>& result, int width, const std::vector<Tile>& tiles) {
    // Cada tile escribe únicamente en su región de la salida.
    for (const Tile& tile : tiles) {
        for (int y = tile.yBegin; y < tile.yEnd; ++y) {
            for (int x = tile.xBegin; x < tile.xEnd; ++x) {
                const std::size_t i = static_cast<std::size_t>(y) * width + x;
                result[i] = conjugateProduct(a[i], b[i]);
            }
        }
    }
}
} // namespace interferogram
