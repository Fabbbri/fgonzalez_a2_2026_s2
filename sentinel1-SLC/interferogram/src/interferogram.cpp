#include "interferogram/interferogram.hpp"
#include <stdexcept>

namespace interferogram {
void validateInterferogramInputs(const std::vector<Complex>& a,
                                const std::vector<Complex>& b,
                                const std::vector<Complex>& result) {
    if (a.empty() || a.size() != b.size() || a.size() != result.size() ||
        &a == &result || &b == &result)
        throw std::invalid_argument("El kernel requiere entradas no vacías y salida independiente del mismo tamaño.");
}
void computeInterferogramSequential(const std::vector<Complex>& a,
                                   const std::vector<Complex>& b,
                                   std::vector<Complex>& result) {
    // Future parallelization boundary: independent iterations of this loop.
    for (std::size_t i = 0; i < a.size(); ++i) {
        result[i].real = a[i].real * b[i].real + a[i].imag * b[i].imag;
        result[i].imag = a[i].imag * b[i].real - a[i].real * b[i].imag;
    }
}
} // namespace interferogram
