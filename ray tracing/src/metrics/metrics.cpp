/**
 * @file metrics.cpp
 * Estadística descriptiva, speedup S = Tseq/Tmodelo y eficiencia E = S/P.
 * P cuenta trabajadores reales y excluye el coordinador. El IC95 normal usa
 * 1.96 errores estándar; el modo compartido propaga variación de ambas medias.
 */
#include "metrics/metrics.hpp"

#include <cmath>
#include <numeric>
#include <stdexcept>

namespace rt {
namespace {
// Calcula estadística muestral de una serie ya validada con al menos dos elementos.
Estimate estimate(const std::vector<double>& values) {
    // Convierte n a double para las divisiones de media y varianza.
    const double n = static_cast<double>(values.size());
    // Media aritmética: suma de observaciones dividida por su cantidad.
    const double mean = std::accumulate(values.begin(), values.end(), 0.0) / n;
    // Acumulador de desviaciones cuadráticas respecto a la media.
    double sum_squared = 0.0;
    // Segunda pasada para sumar (x_i-media)², sin modificar las observaciones.
    for (double value : values) sum_squared += (value - mean) * (value - mean);
    // Desviación muestral con corrección de Bessel, denominador n-1.
    const double deviation = std::sqrt(sum_squared / (n - 1.0));
    // IC normal aproximado del 95 % para la MEDIA, no rango de ejecuciones.
    // Con pocas muestras, la aproximación normal tiene precisión limitada.
    // Semiancho del IC95 normal aproximado: 1.96*s/sqrt(n).
    const double margin = 1.96 * deviation / std::sqrt(n);
    return {mean, deviation, mean - margin, mean + margin};
}
} // namespace
// Comparación pareada: se conserva el cociente propio de cada repetición.
Metrics summarize(const std::vector<Measurement>& samples, std::size_t workers) {
    if (samples.size() < 2 || workers == 0)
        throw std::invalid_argument("Se necesitan al menos 2 mediciones y P positivo");
    // Series separadas para estimar tiempo y métricas derivadas.
    std::vector<double> total, sequential, speedup, efficiency;
    // Valida valores y transforma cada par en aceleración y eficiencia.
    for (const auto& sample : samples) {
        // Rechaza NaN, infinitos y tiempos no positivos, evitando cocientes indefinidos.
        if (!std::isfinite(sample.total_ms) || !std::isfinite(sample.sequential_ms) ||
            sample.total_ms <= 0 || sample.sequential_ms <= 0)
            throw std::invalid_argument("Los tiempos deben ser finitos y positivos");
        // Speedup de una observación: tiempo secuencial dividido por tiempo del modelo.
        const double acceleration = sample.sequential_ms / sample.total_ms;
        total.push_back(sample.total_ms);
        sequential.push_back(sample.sequential_ms);
        speedup.push_back(acceleration);
        // E = S/P; no equivale a ocupación de CPU y puede superar uno por efectos reales o ruido.
        efficiency.push_back(acceleration / workers);
    }
    return {estimate(total), estimate(sequential), estimate(speedup), estimate(efficiency)};
}
// Comparación con referencia común; reemplaza la estimación pareada por razón de medias.
Metrics summarize_shared(const std::vector<Measurement>& samples,
                         const std::vector<double>& baseline, std::size_t workers) {
    auto result = summarize(samples, workers); // Valida tiempos y P.
    if (baseline.size() < 2) throw std::invalid_argument("Referencia requiere al menos 2 tiempos");
    for (double t : baseline) if (!std::isfinite(t) || t <= 0)
        throw std::invalid_argument("Referencia inválida");
    // Estima la referencia a partir de sus n_base tiempos originales, sin duplicarlos.
    result.sequential = estimate(baseline);
    // Alias de la estimación de referencia; t identifica la del modelo.
    const auto& b = result.sequential;
    const auto& t = result.total;
    // Aceleración global Bmedia/Tmedia, diferente de promediar cocientes por fila.
    const double ratio = b.mean / t.mean;
    // Variación relativa de la referencia: (s_base/media_base)².
    const double vb = std::pow(b.deviation / b.mean, 2);
    // Variación relativa del modelo: (s_modelo/media_modelo)².
    const double vt = std::pow(t.deviation / t.mean, 2);
    // Método delta para Bmedia/Tmedia; no son pares ni muestras independientes
    // de speedup. La misma referencia se comparte entre configuraciones.
    // Dispersión propagada por delta de las variables, no error estándar de la media.
    const double deviation = ratio * std::sqrt(vb + vt);
    // Para el IC se divide cada varianza relativa por su propio número de muestras.
    const double margin = 1.96 * ratio * std::sqrt(vb / baseline.size() + vt / samples.size());
    // Asigna estimación y límites a S; la división posterior por P obtiene E.
    result.speedup = {ratio, deviation, ratio - margin, ratio + margin};
    result.efficiency = {ratio / workers, deviation / workers,
                         (ratio - margin) / workers, (ratio + margin) / workers};
    return result;
}
} // namespace rt
