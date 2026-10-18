#include "interferogram/models/fine_grained.hpp"
#include "interferogram/kernel.hpp"
#include <stdexcept>

namespace interferogram {
/*
 * Prepara una tarea por tile; no crea hilos.
 *
 * @param quantum Píxeles que procesa una tarea antes de ceder el turno.
 * @throws std::invalid_argument si quantum no es positivo o los tiles son inválidos.
 */
FineGrained::FineGrained(int width, int height, int tileWidth, int tileHeight, int quantum)
    : width_(width), quantum_(quantum) {
    if (quantum <= 0)
        throw std::invalid_argument("El quantum debe ser positivo.");
    // Cada tile se convierte en una tarea que recuerda el próximo píxel a procesar.
    for (const auto& tile : makeTiles(width, height, tileWidth, tileHeight))
        tasks_.push_back({tile, tile.xBegin, tile.yBegin});
    reset();
}

// Devuelve cada tarea al inicio de su tile para repetir el cálculo completo.
void FineGrained::reset() {
    for (auto& task : tasks_) {
        task.x = task.tile.xBegin;
        task.y = task.tile.yBegin;
    }
    // Todas las tareas vuelven a estar pendientes y los contadores se reinician.
    pending_ = tasks_.size();
    rounds_ = 0;
    dispatches_ = 0;
}

/*
 * Ejecuta una ronda: cada tarea pendiente avanza hasta quantum píxeles.
 * El llamador repite rondas hasta que finished() devuelve true.
 */
void FineGrained::runRound(const std::vector<Complex>& a, const std::vector<Complex>& b,
                           std::vector<Complex>& result) {
    if (finished()) return; // Nada que hacer: no contar una ronda vacía.
    ++rounds_;
    // Round-robin: ceder por quantum, sin bloqueos.
    for (auto& task : tasks_) {
        if (task.y == task.tile.yEnd) continue; // Tarea terminada.
        ++dispatches_; // La tarea recibe un turno.
        // Procesar hasta quantum píxeles o hasta terminar el tile, lo que ocurra primero.
        for (int processed = 0; processed < quantum_ && task.y < task.tile.yEnd; ++processed) {
            // Índice lineal en los vectores de la ventana (orden por filas).
            const std::size_t i = static_cast<std::size_t>(task.y) * width_ + task.x;
            result[i] = conjugateProduct(a[i], b[i]);
            // Avanzar en orden de filas; al terminar la fila del tile, bajar a la siguiente.
            if (++task.x == task.tile.xEnd) {
                task.x = task.tile.xBegin;
                ++task.y;
            }
        }
        // Si la tarea terminó en este turno, deja de contar como pendiente.
        if (task.y == task.tile.yEnd) --pending_;
    }
}
} // namespace interferogram
