/**
 * @file camera.cpp
 * Construcción de una cámara orientada hacia un punto de mira.
 * La proyección usa campo de visión vertical de 36 grados y conserva
 * la relación de aspecto de la imagen.
 */
#include "scene/camera.hpp"
#include <cmath>
#include <stdexcept>

namespace rt {
Camera::Camera(int width, int height, Vec3 origin, Vec3 target)
    : width_(width), height_(height), origin_(origin) {
    if (width <= 0 || height <= 0) throw std::invalid_argument("Resolución inválida");
    // Vector desde el origen de cámara hacia el punto observado.
    const Vec3 direction = target - origin;
    // Evita normalizar un vector casi nulo, que no define orientación.
    if (dot(direction, direction) < 1e-12)
        throw std::invalid_argument("La cámara y su punto de mira deben ser distintos");
    // Eje longitudinal unitario de la cámara.
    forward_ = normalized(direction);
    // Producto cruz: construye ejes perpendiculares para orientar el plano de imagen.
    const auto cross = [](Vec3 a, Vec3 b) -> Vec3 {
        return {a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
    };
    // Cambia la referencia vertical si resulta casi paralela a forward_,
    // evitando un producto cruz nulo.
    const Vec3 reference_up = std::abs(forward_.y) > 0.999 ? Vec3{0,0,1} : Vec3{0,1,0};
    // Eje horizontal unitario perpendicular al eje longitudinal.
    right_ = normalized(cross(forward_, reference_up));
    // Completa la base ortogonal con el eje vertical de la cámara.
    up_ = cross(right_, forward_);
    // Constante de conversión entre grados y radianes disponible al compilar.
    constexpr double pi = 3.14159265358979323846;
    // tan(FOV/2) da la semialtura del plano de imagen a distancia unitaria.
    half_height_ = std::tan(36.0 * pi / 360.0); // FOV vertical moderado de 36 grados.
    // Ajusta el semiancho mediante W/H para preservar proporciones.
    half_width_ = half_height_ * width / height;
}
Ray Camera::primary_ray(int x, int y, double offset_x, double offset_y) const {
    // Se invierte Y porque la imagen crece hacia abajo.
    // Normaliza la coordenada subpíxel horizontal a [-1,1] y escala el plano.
    const double u = (2.0 * (x + offset_x) / width_ - 1.0) * half_width_;
    // Invierte el eje vertical de la imagen al sistema de coordenadas de cámara.
    const double v = (1.0 - 2.0 * (y + offset_y) / height_) * half_height_;
    // Combina los ejes de la base y normaliza la dirección del rayo primario.
    return {origin_, normalized(forward_ + right_ * u + up_ * v)};
}

} // namespace rt
