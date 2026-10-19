/**
 * @file sequential.cpp
 * Referencia secuencial de cálculo de aceleraciones.
 * El bucle exterior i identifica el cuerpo receptor y constituye la dimensión
 * que distribuyen coarse, SMT y CMP. El bucle interno j de fuentes se conserva
 * en el mismo orden para comparar resultados numéricos entre modelos.
 */
#include "nbody.hpp"

namespace nbody {
namespace {
// Referencia permanente: un hilo con el mismo contrato de entrada y salida.
// Implementa el mismo contrato de fase que los modelos paralelos, sin crear hilos.
class SequentialModel final : public ExecutionModel {
public:
    // Nombre de la referencia utilizado por la campaña.
    const char* name() const override { return "sequential"; }
    // Un ejecutor real, que también coordina el recorrido.
    std::size_t workers() const override { return 1; }
    // La entrada es inmutable y la salida se escribe en un vector separado.
    void compute_accelerations(const std::vector<Body>& bodies,
                               std::vector<Vec2>& accelerations,
                               double gravity, double epsilon) override {
        // Prepara un elemento de salida por cuerpo antes del recorrido.
        accelerations.resize(bodies.size());
        // Dominio de reparto: i en [0,N). Coarse, SMT y CMP subdividen este bucle exterior.
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            // Calcula toda la suma sobre fuentes j para un único receptor i.
            accelerations[i] = acceleration_for_body(bodies, i, gravity, epsilon);
        }
    }
};
}  // namespace

// Fábrica que devuelve una estrategia mediante su interfaz abstracta.
std::unique_ptr<ExecutionModel> make_sequential_model() {
    return std::make_unique<SequentialModel>();
}
}  // namespace nbody
