#include "interferogram/models/smt.hpp"
#include <limits>
#include <stdexcept>

namespace interferogram {
/*
 * Decide cuántos trabajadores usa SMT.
 * Si requested es 0: con procesadores de un mismo nucleo - SMT disponibles usa todas las CPUs lógicas;
 * sin ellos, usa 2 * núcleos para sobresuscribir y aproximar SMT con hilos del SO.
 *
 * @throws std::invalid_argument si la topología o requested son inválidos.
 */
int selectSmtWorkers(int requested, std::size_t physicalCores, std::size_t logicalCpus) {
    // logicalCpus == 0 significa "desconocido"; si se conoce, no puede ser menor que los núcleos.
    if (physicalCores == 0 || requested < 0 || (logicalCpus && logicalCpus < physicalCores)) {
        throw std::invalid_argument("Topología o cantidad de hilos inválida para SMT.");
    }
    // Resuelve la cantidad de trabajadores cuando requested es cero.
    if (requested == 0) {
        // El resultado se devuelve como int: comprobar que cabe antes de convertir.
        const auto limit = static_cast<std::size_t>(std::numeric_limits<int>::max());
        // Hay hermanos SMT: un trabajador por CPU lógica.
        if (logicalCpus > physicalCores) {
            if (logicalCpus > limit) throw std::invalid_argument("Demasiadas CPUs lógicas.");
            requested = static_cast<int>(logicalCpus);
        } else {
            // Sin SMT: duplicar los núcleos con hilos del SO.
            if (physicalCores > limit / 2) throw std::invalid_argument("Demasiados núcleos para elegir 2*C.");
            requested = static_cast<int>(physicalCores * 2);
        }
    }
    return requested;
}

/*
 * Elige las CPUs de cada trabajador y crea un pool de CoarseGrained con
 * afinidad fija y reparto dinámico de tiles.
 */
Smt::Smt(int width, int height, int tileWidth, int tileHeight, int threads)
    : topology_(detectCpuInfo()) {
    // threads == 0 deja que selectSmtWorkers elija según la topología.
    const int workers = selectSmtWorkers(threads, topology_.physicalCores, topology_.allowedCpus.size());
    // Ordenar por nivel: primero una CPU de cada núcleo y luego sus hermanos SMT.
    // Así los primeros trabajadores no comparten núcleo mientras haya núcleos libres.
    std::vector<int> order;
    for (std::size_t level = 0;; ++level) {
        bool found = false;
        // level 0 toma la primera CPU de cada núcleo, level 1 la segunda, etc.
        for (const auto& group : topology_.siblingGroups) {
            if (level < group.size()) { order.push_back(group[level]); found = true; }
        }
        if (!found) break; // Ningún núcleo tiene más hermanos.
    }
    // Con SMT apagado, repetir CPUs físicas para conservar el mismo P.
    for (int i = 0; i < workers; ++i) cpus_.push_back(order[i % order.size()]);
    // true: tiles dinámicos, para que un hermano SMT más lento no retrase al resto.
    pool_ = std::make_unique<CoarseGrained>(width, height, tileWidth, tileHeight, workers, cpus_, true);
}

// Delegar en el pool; SMT solo cambia la asignación de CPUs y el reparto de tiles.
void Smt::compute(const std::vector<Complex>& a, const std::vector<Complex>& b,
                  std::vector<Complex>& result) {
    // Al regresar, todos los trabajadores han terminado de escribir sus regiones.
    pool_->compute(a, b, result);
}
}
