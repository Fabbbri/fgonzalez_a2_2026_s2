#pragma once
#include "interferogram/interferogram.hpp"
#include <cstddef>

namespace interferogram {
// Una tarea por tile. Simula el intercambio de hilos en un solo hilo del SO:
// en cada ronda, cada tarea pendiente procesa hasta quantum píxeles y cede el turno.
class FineGrained {
public:
    FineGrained(int width, int height, int tileWidth, int tileHeight, int quantum);
    void reset(); // Reinicia contextos y estadísticas para repetir el cálculo.
    // Requiere vectores validados de width * height muestras.
    void runRound(const std::vector<Complex>& a, const std::vector<Complex>& b,
                  std::vector<Complex>& result);
    bool finished() const { return pending_ == 0; }
    std::size_t tasks() const { return tasks_.size(); }
    std::size_t rounds() const { return rounds_; }
    std::size_t dispatches() const { return dispatches_; }
private:
    struct Task { Tile tile; int x, y; }; // (x, y): siguiente píxel dentro del tile.
    int width_, quantum_;
    std::vector<Task> tasks_;
    // dispatches_ cuenta cada vez que una tarea recibe turno.
    std::size_t pending_ = 0, rounds_ = 0, dispatches_ = 0;
};
} // namespace interferogram
