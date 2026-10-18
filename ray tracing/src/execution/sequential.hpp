/**
 * @file sequential.hpp
 * Estadísticas, callbacks y contrato del recorrido secuencial.
 * Los callbacks desacoplan el cálculo de SDL; pertenecen al hilo coordinador.
 * Una región notificada está completa, aunque otros tiles sigan en ejecución.
 */
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
    // Sondeo opcional en el coordinador; false solicita terminar el renderizado.
    std::function<bool()> keep_running;
    // Notificación opcional de una región completamente escrita; recibe sus límites.
    std::function<void(const Tile&)> tile_completed;
};
struct RenderStats {
    // render_time: cálculo acumulado para sequential; pared para los demás modelos.
    // loop_time es el tiempo de pared comparable entre modelos en modo headless.
    // Número de tiles completos, usado para distinguir imagen final de cancelada.
    std::size_t completed = 0;
    // Duración nativa: cálculo descontando eventos en sequential; pared en los demás.
    std::chrono::steady_clock::duration render_time{};
    // Tiempo de pared del modelo, utilizado por benchmark para comparar ejecuciones.
    std::chrono::steady_clock::duration loop_time{};
    // Hilos de cálculo reales; no cuenta el coordinador adicional de coarse/SMT/CMP.
    std::size_t workers = 1;
    // Número de turnos concedidos por el planificador cooperativo fine.
    std::size_t quanta = 0; // Turnos cooperativos de fine-grained.
    // Número total de esperas simuladas por los trabajadores coarse.
    std::size_t blocks = 0; // Esperas simuladas de coarse-grained.
};
// Referencia sin paralelismo con la misma escena, cámara, tiles y cálculo de píxel.
// Recorre todos los índices de tile en orden y devuelve estadísticas de la referencia.
RenderStats render_sequential(const Scene& scene, const Camera& camera, Image& image,
                              const TileGrid& tiles, const RenderHooks& hooks = {});
} // namespace rt
