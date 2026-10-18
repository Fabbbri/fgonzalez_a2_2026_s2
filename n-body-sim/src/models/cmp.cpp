/**
 * @file cmp.cpp
 * Modelo CMP: un destino lógico por núcleo físico disponible.
 * Reutiliza el grupo persistente y la partición de cuerpos de coarse, con
 * bloqueos temporizados desactivados. P no supera los núcleos físicos.
 * En Linux fija afinidad; en macOS la topología no aporta destinos equivalentes.
 */
#include "nbody.hpp"
#include <stdexcept>

namespace nbody {
namespace {
// Mismo kernel y pool que Coarse, restringiendo trabajadores a núcleos físicos
// distintos. Linux fija una CPU lógica por núcleo, sin usar sus hermanos SMT.
// Adaptador de política física de CPU; no cambia kernel ni partición estática de i.
class CmpModel final : public ExecutionModel {
    // Propiedad exclusiva del grupo persistente reutilizado.
    std::unique_ptr<ExecutionModel> pool_;
public:
    // Recibe P ya validado contra C; selecciona los primeros P núcleos disponibles.
    CmpModel(std::size_t workers, const CpuTopology& topology) {
        // Copia una CPU lógica por núcleo; puede estar vacía en la ruta de macOS.
        auto cpus = topology.core_cpus;
        // Acota destinos a P sin duplicar hermanos de un mismo núcleo.
        if (!cpus.empty()) cpus.resize(workers);
        // Intervalo y duración cero desactivan bloqueos simulados al reutilizar coarse.
        pool_ = make_coarse_grained_model(workers, cpus, 0, 0);
    }
    // El adaptador informa cmp aunque la implementación del grupo sea compartida.
    const char* name() const override { return "cmp"; }
    // P real se consulta al grupo propietario de los hilos.
    std::size_t workers() const override { return pool_->workers(); }
    // Reexpone las estadísticas de las fases completadas por el grupo.
    SchedulingStats scheduling_stats() const override { return pool_->scheduling_stats(); }
    // Delegación íntegra al grupo: misma ecuación y orden de fuentes por receptor.
    void compute_accelerations(const std::vector<Body>& bodies,
                               std::vector<Vec2>& accelerations,
                               double gravity, double epsilon) override {
        pool_->compute_accelerations(bodies, accelerations, gravity, epsilon);
    }
};
}  // namespace

// Fábrica que detecta recursos, selecciona P automático y rechaza sobresuscripción.
std::unique_ptr<ExecutionModel> make_cmp_model(std::size_t workers) {
    const auto topology = detect_cpu_topology();
    // El centinela cero solicita todos los C núcleos físicos disponibles.
    if (workers == 0) workers = topology.physical_cores;
    // CMP conserva como máximo un trabajador por núcleo físico.
    if (workers > topology.physical_cores) {
        throw std::invalid_argument("CMP requiere --threads entre 1 y los núcleos físicos disponibles (" +
                                    std::to_string(topology.physical_cores) + ").");
    }
    return std::make_unique<CmpModel>(workers, topology);
}
}  // namespace nbody
