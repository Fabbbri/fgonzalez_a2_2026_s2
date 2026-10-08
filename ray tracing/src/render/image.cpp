#include "render/image.hpp"
#include <fstream>
#include <stdexcept>

namespace rt {
Image::Image(int width, int height) : width_(width), height_(height) {
    if (width <= 0 || height <= 0) throw std::invalid_argument("Resolución inválida");
    const auto w = static_cast<std::size_t>(width);
    if (w > pixels_.max_size() / static_cast<std::size_t>(height))
        throw std::length_error("Imagen demasiado grande");
    pixels_.resize(w * height); // Negro indica píxeles todavía no calculados.
}
Pixel& Image::at(int x, int y) { return pixels_[static_cast<std::size_t>(y)*width_ + x]; }
void Image::save_ppm(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("No se pudo abrir: " + path);
    file << "P6\n" << width_ << ' ' << height_ << "\n255\n";
    for (int y = 0; y < height_; ++y)
        file.write(reinterpret_cast<const char*>(pixels_.data() + static_cast<std::size_t>(y)*width_),
                   static_cast<std::streamsize>(width_) * sizeof(Pixel));
    file.close(); // Incluye el vaciado del stream en el tiempo de escritura.
    if (!file) throw std::runtime_error("Error escribiendo: " + path);
}

} // namespace rt
