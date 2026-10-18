/**
 * @file display.hpp
 * Interfaz de presentación SDL independiente de geometría y planificación.
 * El patrón PImpl oculta los tipos SDL y permite compilar sin biblioteca gráfica.
 */
#pragma once
#include "render/image.hpp"
#include "render/tiles.hpp"
#include <memory>

namespace rt {
// La presentación desconoce esferas, cámara e iluminación.
class Display {
public:
    // Crea ventana, renderizador de presentación y textura del tamaño indicado.
    Display(int width, int height);
    // Libera los recursos mediante la implementación privada.
    ~Display();
    // Atiende eventos sin bloqueo; false representa petición de cierre.
    bool poll();
    // Transfiere imagen completa o rectángulo terminado; solo se llama desde el coordinador.
    void present(const Image& image, const Tile* changed = nullptr);
    // Bloquea en eventos tras presentar el resultado para evitar espera activa.
    void wait_until_closed();
private:
    // Declaración adelantada: los tipos SDL se definen únicamente en display.cpp.
    struct Impl;
    // Propiedad exclusiva con destrucción automática del estado gráfico.
    std::unique_ptr<Impl> impl_;
};
} // namespace rt
