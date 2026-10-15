#pragma once

#include "scene/geometry.hpp"
#include <algorithm>
#include <cstdint>

namespace rt {
// Generador local SplitMix64: no hay estado global compartido entre píxeles.
// Las operaciones enteras fijas hacen reproducibles las muestras por semilla.
// FUTURA PARALELIZACIÓN: cada píxel crea su Sampler. No usar el identificador
// del hilo como semilla: cambiar los trabajadores debe conservar la imagen.
class Sampler {
public:
    explicit Sampler(std::uint64_t seed) : state_(seed) {}
    double uniform() {
        std::uint64_t value = (state_ += 0x9e3779b97f4a7c15ULL);
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        value ^= value >> 31;
        return static_cast<double>(value >> 11) * (1.0 / 9007199254740992.0);
    }
    Vec3 unit_vector() {
        // Muestreo uniforme de la esfera: Z uniforme y ángulo azimutal uniforme.
        const double z = 2.0 * uniform() - 1.0;
        const double angle = 6.28318530717958647692 * uniform();
        const double radius = std::sqrt(std::max(0.0, 1.0 - z*z));
        return {radius * std::cos(angle), radius * std::sin(angle), z};
    }
private:
    std::uint64_t state_;
};
} // namespace rt
