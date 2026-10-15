#pragma once
#include "interferogram/cpu_info.hpp"
#include "interferogram/models/coarse_grained.hpp"
#include <memory>

namespace interferogram {
// requested == 0 elige automáticamente 2 * núcleos físicos disponibles.
int selectSmtWorkers(int requested, std::size_t physicalCores);

// Primero un trabajador por núcleo.
// Reutiliza el pool de CoarseGrained para procesar tiles con más hilos que núcleos.
class Smt {
public:
    Smt(int width, int height, int tileWidth, int tileHeight, int threads = 0);
    // result debe estar dimensionado. No llamar simultáneamente sobre el mismo modelo.
    void compute(const std::vector<Complex>& a, const std::vector<Complex>& b,
                 std::vector<Complex>& result);
    const CpuInfo& topology() const {
        return topology_;
    }
    const std::vector<int>& cpus() const { return cpus_; }
    std::size_t workers() const {
        return pool_->workers();
    }
    // Cuenta tiles, no hilos: un trabajador puede recibir varios o ninguno.
    std::size_t tasks() const {
        return pool_->tasks();
    }
private:
    CpuInfo topology_;
    std::vector<int> cpus_;
    std::unique_ptr<CoarseGrained> pool_;
};
}
