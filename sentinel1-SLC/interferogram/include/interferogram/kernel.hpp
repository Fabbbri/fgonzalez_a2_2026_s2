#pragma once
#include "complex.hpp"

namespace interferogram {
// Operación común a todos los modelos: A * conjugado(B).
inline Complex conjugateProduct(const Complex& a, const Complex& b) {
    return {a.real * b.real + a.imag * b.imag,
            a.imag * b.real - a.real * b.imag};
}
} // namespace interferogram
