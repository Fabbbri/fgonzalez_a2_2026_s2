#include "metrics/metrics.hpp"

#include <cmath>
#include <numeric>
#include <stdexcept>

namespace rt {
namespace {
Estimate estimate(const std::vector<double>& values) {
    const double n = static_cast<double>(values.size());
    const double mean = std::accumulate(values.begin(), values.end(), 0.0) / n;
    double sum_squared = 0.0;
    for (double value : values) sum_squared += (value - mean) * (value - mean);
    const double deviation = std::sqrt(sum_squared / (n - 1.0));
    // IC normal aproximado del 95 % para la MEDIA, no rango de ejecuciones.
    // Con pocas muestras es solo diagnóstico; la referencia del enunciado es 200.
    const double margin = 1.96 * deviation / std::sqrt(n);
    return {mean, deviation, mean - margin, mean + margin};
}
} // namespace
Metrics summarize(const std::vector<Measurement>& samples, std::size_t workers) {
    if (samples.size() < 2 || workers == 0)
        throw std::invalid_argument("Se necesitan al menos 2 mediciones y P positivo");
    std::vector<double> total, sequential, speedup, efficiency;
    for (const auto& sample : samples) {
        if (!std::isfinite(sample.total_ms) || !std::isfinite(sample.sequential_ms) ||
            sample.total_ms <= 0 || sample.sequential_ms <= 0)
            throw std::invalid_argument("Los tiempos deben ser finitos y positivos");
        const double acceleration = sample.sequential_ms / sample.total_ms;
        total.push_back(sample.total_ms);
        sequential.push_back(sample.sequential_ms);
        speedup.push_back(acceleration);
        efficiency.push_back(acceleration / workers);
    }
    return {estimate(total), estimate(sequential), estimate(speedup), estimate(efficiency)};
}
Metrics summarize_shared(const std::vector<Measurement>& samples,
                         const std::vector<double>& baseline, std::size_t workers) {
    auto result = summarize(samples, workers); // Valida tiempos y P.
    if (baseline.size() < 2) throw std::invalid_argument("Referencia requiere al menos 2 tiempos");
    for (double t : baseline) if (!std::isfinite(t) || t <= 0)
        throw std::invalid_argument("Referencia inválida");
    result.sequential = estimate(baseline);
    const auto& b = result.sequential;
    const auto& t = result.total;
    const double ratio = b.mean / t.mean;
    const double vb = std::pow(b.deviation / b.mean, 2);
    const double vt = std::pow(t.deviation / t.mean, 2);
    // Método delta para Bmedia/Tmedia; no son pares ni muestras independientes
    // de speedup. La misma referencia se comparte entre configuraciones.
    const double deviation = ratio * std::sqrt(vb + vt);
    const double margin = 1.96 * ratio * std::sqrt(vb / baseline.size() + vt / samples.size());
    result.speedup = {ratio, deviation, ratio - margin, ratio + margin};
    result.efficiency = {ratio / workers, deviation / workers,
                         (ratio - margin) / workers, (ratio + margin) / workers};
    return result;
}
} // namespace rt
