/**
 * @file sampling.hpp
 * Generador SplitMix64 local y muestreo uniforme de direcciones esféricas.
 * Su estado privado evita contención entre trabajadores; la semilla de cada
 * píxel depende de sus coordenadas y de la semilla de escena.
 */
#pragma once

#include "scene/geometry.hpp"
#include <algorithm>
#include <cstdint>

namespace rt {
// Generador local SplitMix64: no hay estado global compartido entre píxeles.
// Las operaciones enteras fijas hacen reproducibles las muestras por semilla.
// Cada píxel crea su Sampler con una semilla independiente del hilo,
// por lo que cambiar la cantidad de trabajadores conserva la imagen.
class Sampler {
public:
    // Inicializa un estado privado de 64 bits; explicit evita conversiones implícitas.
    explicit Sampler(std::uint64_t seed) : state_(seed) {}
    double uniform() {
        // Avanza el estado con la constante de Weyl de SplitMix64; el desbordamiento
        // sin signo corresponde a aritmética módulo 2^64.
        std::uint64_t value = (state_ += 0x9e3779b97f4a7c15ULL);
        // Primera mezcla XOR y multiplicación para difundir bits del estado.
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        // Segunda mezcla que reduce correlación entre patrones de bits próximos.
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        // Mezcla final del generador antes de convertir a punto flotante.
        value ^= value >> 31;
        // Conserva 53 bits, compatibles con la precisión de double, y divide por 2^53.
        return static_cast<double>(value >> 11) * (1.0 / 9007199254740992.0);
    }
    Vec3 unit_vector() {
        // Muestreo uniforme de la esfera: Z uniforme y ángulo azimutal uniforme.
        // Coordenada z uniforme en [-1,1); evita la concentración polar del ángulo uniforme.
        const double z = 2.0 * uniform() - 1.0;
        // Azimut uniforme en [0,2*pi).
        const double angle = 6.28318530717958647692 * uniform();
        // Radio del círculo horizontal: sqrt(1-z²), acotado contra redondeo.
        const double radius = std::sqrt(std::max(0.0, 1.0 - z*z));
        // Conversión cilíndrica a dirección cartesiana de longitud unitaria.
        return {radius * std::cos(angle), radius * std::sin(angle), z};
    }
private:
    // Único estado mutable del generador; pertenece a la trayectoria del píxel.
    std::uint64_t state_;
};
} // namespace rt
