#include "scene/camera.hpp"
#include <cmath>
#include <stdexcept>

namespace rt {
Camera::Camera(int width, int height, Vec3 origin, Vec3 target)
    : width_(width), height_(height), origin_(origin) {
    if (width <= 0 || height <= 0) throw std::invalid_argument("Resolución inválida");
    const Vec3 direction = target - origin;
    if (dot(direction, direction) < 1e-12)
        throw std::invalid_argument("La cámara y su punto de mira deben ser distintos");
    forward_ = normalized(direction);
    // Producto cruz: construye ejes perpendiculares para orientar el plano de imagen.
    const auto cross = [](Vec3 a, Vec3 b) -> Vec3 {
        return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
    };
    const Vec3 reference_up = std::abs(forward_.y) > 0.999 ? Vec3{0,0,1} : Vec3{0,1,0};
    right_ = normalized(cross(forward_, reference_up));
    up_ = cross(right_, forward_);
    constexpr double pi = 3.14159265358979323846;
    half_height_ = std::tan(36.0 * pi / 360.0); // FOV vertical moderado de 36 grados.
    half_width_ = half_height_ * width / height;
}
Ray Camera::primary_ray(int x, int y, double offset_x, double offset_y) const {
    // Se invierte Y porque la imagen crece hacia abajo.
    const double u = (2.0 * (x + offset_x) / width_ - 1.0) * half_width_;
    const double v = (1.0 - 2.0 * (y + offset_y) / height_) * half_height_;
    return {origin_, normalized(forward_ + right_ * u + up_ * v)};
}

} // namespace rt
