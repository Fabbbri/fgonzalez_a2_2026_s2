#pragma once

#include "sequential.hpp"

namespace rt {
enum class ExecutionModel { sequential, fine, coarse };
const char* model_name(ExecutionModel model);

RenderStats render_fine(const Scene& scene, const Camera& camera, Image& image,
                        const TileGrid& tiles, std::size_t quantum,
                        const RenderHooks& hooks = {});
// La propuesta asigna un hilo por tile. El intervalo 0 desactiva bloqueos.
RenderStats render_coarse(const Scene& scene, const Camera& camera, Image& image,
                          const TileGrid& tiles, int block_interval_ms,
                          int block_duration_ms, const RenderHooks& hooks = {},
                          std::size_t worker_limit = 0);
} // namespace rt
