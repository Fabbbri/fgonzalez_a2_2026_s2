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
    std::vector<int> order;
    for (std::size_t level = 0;; ++level) {
        bool found = false;
        for (const auto& group : topology_.siblingGroups) {
            if (level < group.size()) { order.push_back(group[level]); found = true; }
        }
        if (!found) break;
    }
    // Con SMT apagado, repetir CPUs físicas para conservar el mismo P.
    for (int i = 0; i < workers; ++i) cpus_.push_back(order[i % order.size()]);
    pool_ = std::make_unique<CoarseGrained>(width, height, tileWidth, tileHeight, workers, cpus_);
}

void Smt::compute(const std::vector<Complex>& a, const std::vector<Complex>& b,
                  std::vector<Complex>& result) {
    // Al regresar, todos los trabajadores han terminado de escribir sus regiones.
    pool_->compute(a, b, result);
}
}
