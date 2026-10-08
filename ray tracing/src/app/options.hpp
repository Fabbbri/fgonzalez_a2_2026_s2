#pragma once

#include <string>
#include <vector>
#include "execution/execution.hpp"

namespace rt {
struct Options {
    int width = 640, height = 480, tile_width = 32, tile_height = 32;
    int samples = 128, max_bounces = 12;
    bool headless = false, help = false, cpu_info = false;
    std::string output;
    ExecutionModel model = ExecutionModel::sequential;
    int quantum = 64;
    int block_interval_ms = 10, block_duration_ms = 1;
    int threads = 0; // 0 interno: automático según modelo. CLI acepta P positivo.
    bool benchmark = false;
    int repetitions = 200;
    std::vector<int> thread_counts;
    std::string metrics_output = "results/metrics.csv";
};
// Interpreta y valida los argumentos; no inicializa escena ni ventana.
Options parse_options(int argc, char** argv);
} // namespace rt
