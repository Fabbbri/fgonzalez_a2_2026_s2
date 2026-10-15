#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rt {
struct Pixel { std::uint8_t r, g, b; };
static_assert(sizeof(Pixel) == 3, "SDL y PPM requieren RGB contiguo");
class Image {
public:
    Image(int width, int height);
    int width() const { return width_; }
    int height() const { return height_; }
    Pixel& at(int x, int y);
    const Pixel* data() const { return pixels_.data(); }
    void save_ppm(const std::string& path) const;
private:
    int width_, height_;
    std::vector<Pixel> pixels_;
};

} // namespace rt
