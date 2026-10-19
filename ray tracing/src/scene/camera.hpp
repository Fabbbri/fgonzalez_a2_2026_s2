/**
 * @file camera.hpp
 * Cámara perspectiva inmutable durante el cálculo de la imagen.
 * Su base ortogonal transforma coordenadas de píxel en rayos del espacio 3D.
 */
#pragma once

#include "scene/geometry.hpp"

namespace rt {
// Cámara fija durante el renderizado; posición y punto de mira configurables.
class Camera {
public:
    // Construye una base ortogonal y el plano de proyección para W por H píxeles.
    Camera(int width, int height, Vec3 origin = {0, 0, 0}, Vec3 target = {0, 0, -1});
    // 0.5 apunta al centro; otras posiciones permiten antialiasing por muestras.
    // Genera un rayo desde origin_; los offsets sitúan la muestra dentro del píxel.
    Ray primary_ray(int x, int y, double offset_x = 0.5, double offset_y = 0.5) const;
private:
    // Dimensiones del plano discreto para normalizar coordenadas de imagen.
    int width_, height_;
    // Semiextensiones del plano de proyección a distancia unitaria.
    double half_height_, half_width_;
    // Posición y base ortogonal: profundidad, horizontal y vertical.
    Vec3 origin_, forward_, right_, up_;
};

} // namespace rt
