#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <Eigen/Dense>
#include <cstdint>
#include <cstddef>

constexpr uint64_t CHECKSUM_INICIAL = 1469598103934665603ULL;

inline uint64_t hash_bits(const double* datos, size_t n, uint64_t h) {
    const unsigned char* p = reinterpret_cast<const unsigned char*>(datos);
    for (size_t i = 0; i < n * sizeof(double); i++) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

// Sirve para MatrixXd y VectorXd (solo importan los bytes y el orden)
template <typename D>
inline uint64_t hash_params(const Eigen::PlainObjectBase<D>& m, uint64_t h) {
    return hash_bits(m.data(), static_cast<size_t>(m.size()), h);
}

#endif