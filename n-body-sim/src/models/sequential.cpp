#include "nbody.hpp"

namespace nbody {
namespace {
// Referencia permanente: un hilo, misma entrada/salida que los futuros modelos.
class SequentialModel final : public ExecutionModel {
public:
    const char* name() const override { return "sequential"; }
    std::size_t workers() const override { return 1; }
    void compute_accelerations(const std::vector<Body>& bodies,
                               std::vector<Vec2>& accelerations,
                               double gravity, double epsilon) override {
        accelerations.resize(bodies.size());
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            accelerations[i] = acceleration_for_body(bodies, i, gravity, epsilon);
        }
    }
};
}  // namespace

std::unique_ptr<ExecutionModel> make_sequential_model() {
    return std::make_unique<SequentialModel>();
}
}  // namespace nbody
