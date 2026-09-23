#pragma once

#include "geometry.hpp"

namespace rt {
// Cámara fija durante el renderizado; posición y punto de mira configurables.
class Camera {
public:
    Camera(int width, int height, Vec3 origin = {0, 0, 0}, Vec3 target = {0, 0, -1});
    // 0.5 apunta al centro; otras posiciones permiten antialiasing por muestras.
    Ray primary_ray(int x, int y, double offset_x = 0.5, double offset_y = 0.5) const;
private:
    int width_, height_;
    double half_height_, half_width_;
    Vec3 origin_, forward_, right_, up_;
};

} // namespace rt
