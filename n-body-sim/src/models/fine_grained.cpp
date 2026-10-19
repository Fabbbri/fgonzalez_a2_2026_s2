/**
 * @file fine_grained.cpp
 * Planificación cooperativa round-robin en un único hilo real.
 * Existe una tarea por cuerpo receptor i; su contexto conserva next_source
 * como índice de la próxima fuente j y partial como aceleración acumulada.
 * quantum limita interacciones no propias por turno. La suma conserva orden
 * ascendente de j, sin reducción concurrente ni cambio de agrupación aritmética.
 */
#include "nbody.hpp"
#include <algorithm>
#include <stdexcept>

namespace nbody {
namespace {
// Aproximación cooperativa en un hilo del SO. Cada tarea conserva su contexto;
// el cambio de tarea se produce por quantum, sin esperar un bloqueo.
class FineGrainedModel final : public ExecutionModel {
    // Contexto reanudable de un cuerpo receptor; no corresponde a un hilo del SO.
    struct Task {
        // j inicial del próximo intervalo de fuentes, en orden ascendente.
        std::size_t next_source = 0;
        // Acumulador privado a_i que conserva lo sumado durante turnos previos.
        Vec2 partial;
    };
    // K interacciones no propias por visita de una tarea.
    const std::size_t quantum_;
    // Un estado por cuerpo i; se reinicia para cada nueva instantánea física.
    std::vector<Task> tasks_;
    // Acumula rondas y despachos entre fases; tasks representa el N de la fase actual.
    SchedulingStats stats_;
public:
    // Un quantum positivo garantiza avance de las tareas en cada ronda.
    explicit FineGrainedModel(std::size_t quantum) : quantum_(quantum) {
        if (quantum == 0) throw std::invalid_argument("El quantum debe ser positivo.");
    }
    // Etiqueta textual estable del modelo cooperativo.
    const char* name() const override { return "fine-grained"; }
    // Devuelve un hilo real aunque existan N tareas lógicas.
    std::size_t workers() const override { return 1; }  // Un hilo real, N tareas simuladas.
    // Expone contadores lógicos; no son mediciones de cambios de contexto del SO.
    SchedulingStats scheduling_stats() const override { return stats_; }

    // Intercala intervalos de fuentes por cuerpo hasta completar toda la fase.
    void compute_accelerations(const std::vector<Body>& bodies,
                               std::vector<Vec2>& accelerations,
                               double gravity, double epsilon) override {
        // N es el número total de cuerpos de las dos galaxias.
        const std::size_t count = bodies.size();
        // Cada fase descarta parciales anteriores porque las posiciones fuente han cambiado.
        tasks_.assign(count, Task{});  // Nuevo estado físico: reiniciar todos los contextos.
        // Inicializa salida; cada elemento recibe su acumulado solo al terminar su tarea.
        accelerations.assign(count, Vec2{});
        stats_.tasks = count;
        // Número de cuerpos aún incompletos; termina en cero al agotar sus fuentes.
        std::size_t pending = count;
        // Cada iteración concede como máximo un turno a cada tarea activa.
        while (pending > 0) {
            ++stats_.rounds;
            // Round-robin determinista: una visita por tarea pendiente y ronda.
            // i identifica cuerpos receptores visitados en round-robin, dentro de un único hilo.
            for (std::size_t i = 0; i < count; ++i) {
                // Referencia al contexto del receptor i, conservado entre rondas.
                Task& task = tasks_[i];
                // La tarea ya procesó el dominio de fuentes y no recibe nuevos turnos.
                if (task.next_source == count) continue;
                // Extremo exclusivo del intervalo, acotado por fuentes restantes sin desbordar N.
                std::size_t end = task.next_source + std::min(quantum_, count - task.next_source);
                // La interacción consigo misma no consume quantum. Al final
                // puede quedar un turno más corto; nunca se excede el vector.
                // Extiende el intervalo al cruzar la fuente propia, cuya interacción se omite.
                // El presupuesto cuenta aportes de otros cuerpos, no la posición j==i.
                if (task.next_source <= i && i <= end && end < count) ++end;
                // Continúa la suma de fuentes en el mismo orden de la referencia.
                // No reagrupa sumas parciales entre hilos ni paraleliza el bucle j.
                accumulate_accelerations(bodies, i, task.next_source, end,
                                         task.partial, gravity, epsilon);
                // Guarda el siguiente j para reanudar esta tarea en la siguiente ronda.
                task.next_source = end;
                // Cuenta un turno concedido, independiente de cuántas interacciones completó.
                ++stats_.dispatches;
                if (end == count) {
                    // Publica a_i completo; el integrador no accede a la salida hasta retornar la fase.
                    accelerations[i] = task.partial;
                    // Una tarea terminada deja de contribuir al número de contextos activos.
                    --pending;
                }
            }
        }
        // Barrera lógica: solo se retorna cuando todas las tareas terminaron.
        // El integrador compartido puede entonces actualizar las posiciones.
    }
};
}  // namespace

// Fábrica del planificador cooperativo con destrucción automática.
std::unique_ptr<ExecutionModel> make_fine_grained_model(std::size_t quantum) {
    return std::make_unique<FineGrainedModel>(quantum);
}
}  // namespace nbody
