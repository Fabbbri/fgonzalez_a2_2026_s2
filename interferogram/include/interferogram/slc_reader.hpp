#pragma once
#include "complex.hpp"
#include <string>
#include <vector>

namespace interferogram {
inline constexpr int slcWidth = 24445;
inline constexpr int slcHeight = 1500;
// P09-R2: one complex band, 24445 x 1500. Row-major output.
void validateWindow(int x, int y, int width, int height);
std::vector<Complex> readSlcWindow(const std::string& path,
                                 int x, int y, int width, int height);
} // namespace interferogram
