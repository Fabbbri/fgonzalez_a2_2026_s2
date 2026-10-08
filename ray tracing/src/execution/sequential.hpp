#pragma once

#include "render/raytracer.hpp"
#include <chrono>
#include <functional>

namespace rt {
// El recorrido no depende de SDL: la aplicación proporciona acciones opcionales.
// Todos los modelos invocan estos callbacks en el hilo que llamó al renderizador.
// Los modelos con hilos comunican cancelación con atomics; sus trabajadores no usan SDL.
// tile_completed permite leer solo la región terminada; otros tiles pueden seguir escribiendo.
struct RenderHooks {
    std::function<bool()> keep_running;
    std::function<void(const Tile&)> tile_completed;
};
struct RenderStats {
    // render_time: cálculo acumulado para sequential; pared para los demás modelos.
    // loop_time es el tiempo de pared comparable entre modelos en modo headless.
    std::size_t completed = 0;
    std::chrono::steady_clock::duration render_time{};
    std::chrono::steady_clock::duration loop_time{};
    std::size_t workers = 1;
    std::size_t quanta = 0; // Turnos cooperativos de fine-grained.
    std::size_t blocks = 0; // Esperas simuladas de coarse-grained.
};
// Conservar esta función como referencia sin paralelismo. Los nuevos modelos
// tendrán su propia política y reutilizarán escena, cámara, tiles y cálculo de píxel.
RenderStats render_sequential(const Scene& scene, const Camera& camera, Image& image,
                              const TileGrid& tiles, const RenderHooks& hooks = {});
} // namespace rt
