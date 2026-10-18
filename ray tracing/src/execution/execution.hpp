#pragma once

#include "execution/sequential.hpp"
#include "execution/cpu.hpp"

namespace rt {
enum class ExecutionModel { sequential, fine, coarse, smt, cmp };
const char* model_name(ExecutionModel model);

RenderStats render_fine(const Scene& scene, const Camera& camera, Image& image,
                        const TileGrid& tiles, std::size_t quantum,
                        const RenderHooks& hooks = {});
// Por defecto se asigna un hilo por tile. El intervalo 0 desactiva bloqueos.
RenderStats render_coarse(const Scene& scene, const Camera& camera, Image& image,
                          const TileGrid& tiles, int block_interval_ms,
                          int block_duration_ms, const RenderHooks& hooks = {},
                          std::size_t worker_limit = 0);
// Misma cola atómica y render_tile; distinta asignación de trabajadores a CPU.
RenderStats render_cmp(const Scene& scene, const Camera& camera, Image& image,
                       const TileGrid& tiles, const CpuTopology& topology,
                       std::size_t workers = 0, const RenderHooks& hooks = {});
RenderStats render_smt(const Scene& scene, const Camera& camera, Image& image,
                       const TileGrid& tiles, const CpuTopology& topology,
                       std::size_t workers = 0, const RenderHooks& hooks = {});
} // namespace rt
