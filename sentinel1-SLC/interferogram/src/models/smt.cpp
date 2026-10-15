#include "interferogram/models/smt.hpp"
#include <limits>
#include <stdexcept>

namespace interferogram {
int selectSmtWorkers(int requested, std::size_t physicalCores) {
    if (physicalCores == 0 || requested < 0) {
        throw std::invalid_argument("Topología o cantidad de hilos inválida para SMT.");
    }
    // Cero indica selección automática: dos trabajadores por núcleo disponible.
    if (requested == 0) {
        // Comprobar el límite antes de multiplicar y convertir a int.
        if (physicalCores > static_cast<std::size_t>(std::numeric_limits<int>::max() / 2)) {
            throw std::invalid_argument("Demasiados núcleos para elegir 2*C trabajadores.");
        }
        requested = static_cast<int>(physicalCores * 2);
    }
    // Un valor igual a los núcleos físicos sirve como control.
    if (static_cast<std::size_t>(requested) < physicalCores) {
        throw std::invalid_argument("SMT requiere --threads al menos igual a " +
                                    std::to_string(physicalCores) + " núcleos físicos disponibles.");
    }
    return requested;
}

Smt::Smt(int width, int height, int tileWidth, int tileHeight, int threads)
    : topology_(detectCpuInfo()) {
    const int workers = selectSmtWorkers(threads, topology_.physicalCores);
    // Reutilizamos el reparto de tiles y la sincronización de CoarseGrained.
    // El pool se crea una sola vez y sirve para las siguientes llamadas a compute.
    // Cada hilo hereda la afinidad del coordinador. El SO decide dónde ejecutarlo.
    pool_ = std::make_unique<CoarseGrained>(width, height, tileWidth, tileHeight, workers);
}

void Smt::compute(const std::vector<Complex>& a, const std::vector<Complex>& b,
                  std::vector<Complex>& result) {
    // Al regresar, todos los trabajadores han terminado de escribir sus regiones.
    pool_->compute(a, b, result);
}
}
