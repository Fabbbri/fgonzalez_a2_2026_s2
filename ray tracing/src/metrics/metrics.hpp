/**
 * @file metrics.hpp
 * Tipos para muestras temporales y estimaciones estadísticas.
 * Cada Estimate contiene media, desviación y extremos del IC95 aproximado.
 * Se distinguen comparación pareada y cociente de medias con referencia común.
 */
#pragma once

#include <cstddef>
#include <vector>

namespace rt {
// Estadística de una magnitud: media, desviación y límites inferior/superior del IC95.
struct Estimate {
    double mean, deviation, ci_low, ci_high;
};
// Tiempos de referencia y modelo en milisegundos para una repetición.
struct Measurement {
    double sequential_ms, total_ms;
};
// Estimaciones de tiempo del modelo, referencia, speedup y eficiencia, en ese orden.
struct Metrics {
    Estimate total, sequential, speedup, efficiency;
};
// Tseq/Tmodelo y speedup/P, P = trabajadores REALES, sin contar el coordinador.
// Promedia cocientes por pares y sus eficiencias; exige n>=2 y P>0.
Metrics summarize(const std::vector<Measurement>& samples, std::size_t workers);
// Cociente de medias independientes: propagar incertidumbre de AMBAS medias.
// Usa razón de medias y método delta; la referencia completa participa en el error.
Metrics summarize_shared(const std::vector<Measurement>& samples,
                         const std::vector<double>& baseline, std::size_t workers);
} // namespace rt
