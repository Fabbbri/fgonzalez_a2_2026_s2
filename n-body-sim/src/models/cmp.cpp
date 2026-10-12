#include "nbody.hpp"
#include <stdexcept>

namespace nbody {
namespace {
// Mismo kernel y pool que Coarse, restringiendo trabajadores a núcleos físicos
// distintos. Linux fija una CPU lógica por núcleo, sin usar sus hermanos SMT.
class CmpModel final : public ExecutionModel {
    std::unique_ptr<ExecutionModel> pool_;
public:
    CmpModel(std::size_t workers, const CpuTopology& topology) {
        auto cpus = topology.core_cpus;
        if (!cpus.empty()) cpus.resize(workers);
        pool_ = make_coarse_grained_model(workers, cpus);
    }
    const char* name() const override { return "cmp"; }
    std::size_t workers() const override { return pool_->workers(); }
    SchedulingStats scheduling_stats() const override { return pool_->scheduling_stats(); }
    void compute_accelerations(const std::vector<Body>& bodies,
                               std::vector<Vec2>& accelerations,
                               double gravity, double epsilon) override {
        pool_->compute_accelerations(bodies, accelerations, gravity, epsilon);
    }
};
}  // namespace

std::unique_ptr<ExecutionModel> make_cmp_model(std::size_t workers) {
    const auto topology = detect_cpu_topology();
    if (workers == 0) workers = topology.physical_cores;
    if (workers > topology.physical_cores) {
        throw std::invalid_argument("CMP requiere --threads entre 1 y los núcleos físicos disponibles (" +
                                    std::to_string(topology.physical_cores) + ").");
    }
    return std::make_unique<CmpModel>(workers, topology);
}
}  // namespace nbody
