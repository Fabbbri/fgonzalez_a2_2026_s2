#pragma once

#include "raytracer.hpp"
#include <chrono>
#include <functional>

namespace rt {
// El recorrido no depende de SDL: la aplicación proporciona acciones opcionales.
// FUTURA PARALELIZACIÓN: estos callbacks se ejecutan hoy en el hilo principal.
// Una versión con trabajadores deberá comunicar cancelación/finalización de
// forma sincronizada; no llamar a SDL desde ellos ni compartir contadores sin protección.
struct RenderHooks {
    std::function<bool()> keep_running;
    std::function<void(const Tile&)> tile_completed;
};
struct RenderStats {
    // En paralelo, acumular estadísticas por trabajador y combinarlas al terminar.
    // La suma del tiempo de trabajadores no equivale al tiempo total de pared.
    std::size_t completed = 0;
    std::chrono::steady_clock::duration render_time{};
    std::chrono::steady_clock::duration loop_time{};
};
// Conservar esta función como referencia sin paralelismo. Los nuevos modelos
// tendrán su propia política y reutilizarán escena, cámara, tiles y cálculo de píxel.
RenderStats render_sequential(const Scene& scene, const Camera& camera, Image& image,
                              const TileGrid& tiles, const RenderHooks& hooks = {});
} // namespace rt
