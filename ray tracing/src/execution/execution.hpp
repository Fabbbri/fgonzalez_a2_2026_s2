/**
 * @file execution.hpp
 * Contratos de los modelos de ejecución sobre una partición común en tiles.
 * Las referencias const expresan acceso de lectura; Image& recibe las escrituras
 * de regiones exclusivas. Los modelos devuelven estadísticas tras finalizar
 * y reunir sus trabajadores. Un valor de trabajadores igual a cero selecciona
 * la política automática del modelo.
 */
#pragma once

#include "execution/sequential.hpp"
#include "execution/cpu.hpp"

namespace rt {
// Identificador de política; todas reciben la misma geometría y cálculo de píxel.
enum class ExecutionModel { sequential, fine, coarse, smt, cmp };
// Etiqueta textual estable de la enumeración para consola y archivos CSV.
const char* model_name(ExecutionModel model);

// Un ejecutor cooperativo; quantum limita píxeles completos por turno de tile.
RenderStats render_fine(const Scene& scene, const Camera& camera, Image& image,
                        const TileGrid& tiles, std::size_t quantum,
                        const RenderHooks& hooks = {});
// Por defecto se asigna un hilo por tile. El intervalo 0 desactiva bloqueos.
// M hilos estáticos o P trabajadores dinámicos; los bloqueos conservan tile y progreso.
RenderStats render_coarse(const Scene& scene, const Camera& camera, Image& image,
                          const TileGrid& tiles, int block_interval_ms,
                          int block_duration_ms, const RenderHooks& hooks = {},
                          std::size_t worker_limit = 0);
// Misma cola atómica y render_tile; distinta asignación de trabajadores a CPU.
// Selecciona núcleos físicos distintos y distribuye índices con un contador atómico.
RenderStats render_cmp(const Scene& scene, const Camera& camera, Image& image,
                       const TileGrid& tiles, const CpuTopology& topology,
                       std::size_t workers = 0, const RenderHooks& hooks = {});
// Asigna CPUs lógicas o destinos repetidos; utiliza el mismo reparto atómico que CMP.
RenderStats render_smt(const Scene& scene, const Camera& camera, Image& image,
                       const TileGrid& tiles, const CpuTopology& topology,
                       std::size_t workers = 0, const RenderHooks& hooks = {});
} // namespace rt
