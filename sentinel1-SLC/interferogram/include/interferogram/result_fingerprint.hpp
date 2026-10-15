#pragma once
#include "interferogram/complex.hpp"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace interferogram {
// Diagnóstico fuera del cronómetro: no modifica los píxeles del interferograma.
// Canonizar NaN solo para la huella evita depender de su payload con NoData.
inline std::uint64_t resultFingerprint(const std::vector<Complex>& values) {
    std::uint64_t hash = 14695981039346656037ULL;
    for (const auto& value : values) {
        for (double component : {value.real, value.imag}) {
            std::uint64_t bits;
            static_assert(sizeof(bits) == sizeof(component), "Se requieren doubles de 64 bits");
            if (std::isnan(component)) bits = 0x7ff8000000000000ULL;
            else std::memcpy(&bits, &component, sizeof(bits));
            for (int shift = 0; shift < 64; shift += 8) {
                hash ^= (bits >> shift) & 0xff;
                hash *= 1099511628211ULL;
            }
        }
    }
    return hash;
}
}
