/**
 * @file image.hpp
 * Representación contigua de la imagen en RGB de ocho bits por canal.
 * Los píxeles se almacenan por filas; la dirección lógica es y * width + x.
 * La imagen se reserva antes de crear trabajadores y no se redimensiona
 * durante el renderizado, conservando la validez de sus regiones.
 */
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rt {
// Tres canales sin signo de ocho bits; su representación coincide con RGB24.
struct Pixel { std::uint8_t r, g, b; };
// Verificación en compilación: impide padding incompatible con SDL y PPM.
static_assert(sizeof(Pixel) == 3, "SDL y PPM requieren RGB contiguo");
class Image {
public:
    // Reserva W*H elementos contiguos e inicializa la imagen en negro.
    Image(int width, int height);
    // Ancho lógico en píxeles; const impide modificar la imagen al consultarlo.
    int width() const { return width_; }
    // Altura lógica en píxeles.
    int height() const { return height_; }
    // Referencia mutable al píxel (x,y); las coordenadas llegan validadas por el tile.
    Pixel& at(int x, int y);
    // Vista de solo lectura del almacenamiento contiguo, válida mientras no se redimensiona.
    const Pixel* data() const { return pixels_.data(); }
    // Exportación binaria P6; propaga fallos de apertura, escritura o cierre.
    void save_ppm(const std::string& path) const;
private:
    // Dimensiones invariantes durante la ejecución de los trabajadores.
    int width_, height_;
    // Almacenamiento por filas. La independencia es por elemento, no por copia de imagen.
    std::vector<Pixel> pixels_;
};

} // namespace rt
