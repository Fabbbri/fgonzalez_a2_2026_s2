#pragma once

#include <cstddef>
#include <vector>

namespace rt {
struct Estimate {
    double mean, deviation, ci_low, ci_high;
};
struct Measurement {
    double sequential_ms, total_ms;
};
struct Metrics {
    Estimate total, sequential, speedup, efficiency;
};
// Tseq/Tmodelo y speedup/P, P = trabajadores REALES, sin contar el coordinador.
Metrics summarize(const std::vector<Measurement>& samples, std::size_t workers);
// Cociente de medias independientes: propagar incertidumbre de AMBAS medias.
Metrics summarize_shared(const std::vector<Measurement>& samples,
                         const std::vector<double>& baseline, std::size_t workers);
} // namespace rt
