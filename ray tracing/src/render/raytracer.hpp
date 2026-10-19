/**
 * @file raytracer.hpp
 * Operaciones de cálculo de píxeles y avance reanudable de un tile.
 * TileProgress.next es un índice local de píxel; no es el contador global
 * next utilizado para asignar tiles en parallel.cpp.
 */
#pragma once

#include "scene/camera.hpp"
#include "render/image.hpp"
#include "scene/scene.hpp"
#include "render/tiles.hpp"
#include <functional>

namespace rt {
// Calcula un RGB completo a partir de las muestras y trayectorias de (x,y).
Pixel render_pixel(const Scene& scene, const Camera& camera, int x, int y);
// Estado de una tarea de tile. next cuenta píxeles COMPLETOS, no rayos.
struct TileProgress {
    // Región asignada a la tarea; permanece fija aunque fine ceda el turno.
    Tile tile;
    // Índice privado del siguiente píxel local en orden por filas; inicia en cero.
    std::size_t next = 0;
};
// Área real validada del tile; contempla regiones parciales del borde.
std::size_t tile_pixels(Tile tile);
// Avanza como máximo budget píxeles; false significa pendiente o cancelado.
// Avance reanudable: modifica progress.next e image hasta budget píxeles.
// true indica tarea completa; false indica turno agotado o cancelación.
bool render_tile_step(const Scene& scene, const Camera& camera, Image& image,
                      TileProgress& progress, std::size_t budget,
                      const std::function<bool()>& keep_running = {});
// El callback opcional solo atiende cancelación; nunca cambia de tile.
// Consume el tile completo con un presupuesto máximo y estado privado.
// El llamante garantiza que ningún otro trabajador escribe esa región.
bool render_tile(const Scene& scene, const Camera& camera, Image& image, Tile tile,
                 const std::function<bool()>& keep_running = {});
} // namespace rt
