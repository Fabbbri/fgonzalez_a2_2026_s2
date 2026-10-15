#include "interferogram/models/smt.hpp"
#include <limits>
#include <stdexcept>

namespace interferogram {
int selectSmtWorkers(int requested, std::size_t physicalCores, std::size_t logicalCpus) {
    if (physicalCores == 0 || requested < 0 || (logicalCpus && logicalCpus < physicalCores)) {
        throw std::invalid_argument("Topología o cantidad de hilos inválida para SMT.");
    }
    // Resuelve la cantidad de trabajadores cuando requested es cero.
    if (requested == 0) {
        const auto limit = static_cast<std::size_t>(std::numeric_limits<int>::max());
        if (logicalCpus > physicalCores) {
            if (logicalCpus > limit) throw std::invalid_argument("Demasiadas CPUs lógicas.");
            requested = static_cast<int>(logicalCpus);
        } else {
            if (physicalCores > limit / 2) throw std::invalid_argument("Demasiados núcleos para elegir 2*C.");
            requested = static_cast<int>(physicalCores * 2);
        }
    }
    return requested;
}

Smt::Smt(int width, int height, int tileWidth, int tileHeight, int threads)
    : topology_(detectCpuInfo()) {
    const int workers = selectSmtWorkers(threads, topology_.physicalCores, topology_.allowedCpus.size());
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
    pool_ = std::make_unique<CoarseGrained>(width, height, tileWidth, tileHeight, workers, cpus_, true);
}

void Smt::compute(const std::vector<Complex>& a, const std::vector<Complex>& b,
                  std::vector<Complex>& result) {
    // Al regresar, todos los trabajadores han terminado de escribir sus regiones.
    pool_->compute(a, b, result);
}
}
