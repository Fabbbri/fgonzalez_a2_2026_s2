/**
 * @file image.cpp
 * Reserva del buffer RGB y serialización binaria en formato PPM P6.
 * El acceso at no comprueba límites: los recorridos validan su tile antes
 * de escribir. Trabajadores distintos pueden modificar elementos distintos.
 */
#include "render/image.hpp"
#include <fstream>
#include <stdexcept>

namespace rt {
Image::Image(int width, int height) : width_(width), height_(height) {
    if (width <= 0 || height <= 0) throw std::invalid_argument("Resolución inválida");
    // Convierte a tipo de tamaño solo después de validar dimensiones positivas.
    const auto w = static_cast<std::size_t>(width);
    // Comprueba el producto W*H mediante división para evitar desbordamiento.
    if (w > pixels_.max_size() / static_cast<std::size_t>(height))
        throw std::length_error("Imagen demasiado grande");
    // Reserva e inicializa todos los píxeles antes de comenzar cualquier trabajador.
    pixels_.resize(w * height); // Negro indica píxeles todavía no calculados.
}
// Acceso por filas: índice lineal = y*W+x. Devuelve referencia al elemento
// para permitir escritura directa sin reasignar el vector.
Pixel& Image::at(int x, int y) { return pixels_[static_cast<std::size_t>(y)*width_ + x]; }
void Image::save_ppm(const std::string& path) const {
    // Abre en binario: evita transformaciones de bytes del formato P6.
    std::ofstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("No se pudo abrir: " + path);
    // Cabecera PPM: firma, resolución y valor máximo de canal.
    file << "P6\n" << width_ << ' ' << height_ << "\n255\n";
    // Escribe filas completas desde su dirección inicial en el buffer contiguo.
    for (int y = 0; y < height_; ++y)
        // reinterpret_cast expone la representación en bytes sin convertir los canales.
        // Cada fila ocupa W*sizeof(Pixel), y static_assert garantiza tres bytes por píxel.
        file.write(reinterpret_cast<const char*>(pixels_.data() + static_cast<std::size_t>(y)*width_),
                   static_cast<std::streamsize>(width_) * sizeof(Pixel));
    file.close(); // Incluye el vaciado del stream en el tiempo de escritura.
    // Comprueba también errores del cierre o vaciado del archivo.
    if (!file) throw std::runtime_error("Error escribiendo: " + path);
}

} // namespace rt
