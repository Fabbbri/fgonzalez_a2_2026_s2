#include "nbody.hpp"
#include <algorithm>
#include <stdexcept>

namespace nbody {
namespace {
// Aproximación cooperativa en un hilo del SO. Cada tarea conserva su contexto;
// el cambio de tarea se produce por quantum, sin esperar un bloqueo.
class FineGrainedModel final : public ExecutionModel {
    struct Task {
        std::size_t next_source = 0;
        Vec2 partial;
    };
    const std::size_t quantum_;
    std::vector<Task> tasks_;
    SchedulingStats stats_;
public:
    explicit FineGrainedModel(std::size_t quantum) : quantum_(quantum) {
        if (quantum == 0) throw std::invalid_argument("El quantum debe ser positivo.");
    }
    const char* name() const override { return "fine-grained"; }
    std::size_t workers() const override { return 1; }  // Un hilo real, N tareas simuladas.
    SchedulingStats scheduling_stats() const override { return stats_; }

    void compute_accelerations(const std::vector<Body>& bodies,
                               std::vector<Vec2>& accelerations,
                               double gravity, double epsilon) override {
        const std::size_t count = bodies.size();
        tasks_.assign(count, Task{});  // Nuevo estado físico: reiniciar todos los contextos.
        accelerations.assign(count, Vec2{});
        stats_.tasks = count;
        std::size_t pending = count;
        while (pending > 0) {
            ++stats_.rounds;
            // Round-robin determinista: una visita por tarea pendiente y ronda.
            for (std::size_t i = 0; i < count; ++i) {
                Task& task = tasks_[i];
                if (task.next_source == count) continue;
                std::size_t end = task.next_source + std::min(quantum_, count - task.next_source);
                // La interacción consigo misma no consume quantum. Al final
                // puede quedar un turno más corto; nunca se excede el vector.
                if (task.next_source <= i && i <= end && end < count) ++end;
                accumulate_accelerations(bodies, i, task.next_source, end,
                                         task.partial, gravity, epsilon);
                task.next_source = end;
                ++stats_.dispatches;
                if (end == count) {
                    accelerations[i] = task.partial;
                    --pending;
                }
            }
        }
        // Barrera lógica: solo se retorna cuando todas las tareas terminaron.
        // El integrador compartido puede entonces actualizar las posiciones.
    }
};
}  // namespace

std::unique_ptr<ExecutionModel> make_fine_grained_model(std::size_t quantum) {
    return std::make_unique<FineGrainedModel>(quantum);
}
}  // namespace nbody
