#pragma once
#include "complex.hpp"
#include <vector>

namespace interferogram {
// Call once before timing; the kernel performs no allocation or validation.
void validateInterferogramInputs(const std::vector<Complex>& a, 
const std::vector<Complex>& b, const std::vector<Complex>& result);
// Preconditions: nonempty, equal sizes; result must not alias either input.
void computeInterferogramSequential(const std::vector<Complex>& a,
const std::vector<Complex>& b, std::vector<Complex>& result);
} // namespace interferogram
