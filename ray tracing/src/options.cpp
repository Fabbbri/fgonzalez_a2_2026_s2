#include "options.hpp"
#include <charconv>
#include <limits>
#include <stdexcept>

namespace rt {
namespace {
int integer(const std::string& value, bool allow_zero = false) {
    int result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
        result < 0 || (!allow_zero && result == 0))
        throw std::invalid_argument("Entero fuera de rango: " + value);
    return result;
}
} // namespace
Options parse_options(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string flag = argv[i];
        if (flag == "--headless") options.headless = true;
        else if (flag == "--benchmark") options.benchmark = true;
        else if (flag == "--help" || flag == "-h") options.help = true;
        else {
            if (flag != "--width" && flag != "--height" && flag != "--tile-width" &&
                flag != "--tile-height" && flag != "--tile-size" && flag != "--output" &&
                flag != "--samples" && flag != "--bounces" && flag != "--model" &&
                flag != "--quantum" && flag != "--block-interval-ms" && flag != "--block-duration-ms" &&
                flag != "--threads" && flag != "--thread-counts" && flag != "--repetitions" && flag != "--metrics-output")
                throw std::invalid_argument("Opción desconocida: " + flag);
            if (++i == argc) throw std::invalid_argument("Falta valor para " + flag);
            const std::string value = argv[i];
            if (flag == "--model") {
                if (value == "sequential") options.model = ExecutionModel::sequential;
                else if (value == "fine") options.model = ExecutionModel::fine;
                else if (value == "coarse") options.model = ExecutionModel::coarse;
                else throw std::invalid_argument("Modelos: sequential, fine, coarse");
            } else if (flag == "--thread-counts") {
                options.thread_counts.clear();
                std::size_t begin = 0;
                do {
                    const auto end = value.find(',', begin);
                    options.thread_counts.push_back(integer(value.substr(begin, end - begin)));
                    if (end == std::string::npos) break;
                    begin = end + 1;
                } while (true);
            } else if (flag == "--metrics-output") {
                if (value.empty()) throw std::invalid_argument("Ruta CSV vacía");
                options.metrics_output = value;
            } else if (flag == "--output") {
                if (value.empty()) throw std::invalid_argument("Archivo de salida vacío");
                options.output = value;
            } else {
                const int n = integer(value, flag == "--block-interval-ms" || flag == "--block-duration-ms");
                if (flag == "--width") options.width = n;
                if (flag == "--height") options.height = n;
                if (flag == "--samples") options.samples = n;
                if (flag == "--bounces") options.max_bounces = n;
                if (flag == "--quantum") options.quantum = n;
                if (flag == "--threads") options.threads = n;
                if (flag == "--repetitions") options.repetitions = n;
                if (flag == "--block-interval-ms") options.block_interval_ms = n;
                if (flag == "--block-duration-ms") options.block_duration_ms = n;
                if (flag == "--tile-width" || flag == "--tile-size") options.tile_width = n;
                if (flag == "--tile-height" || flag == "--tile-size") options.tile_height = n;
            }
        }
    }
    if (options.width > std::numeric_limits<int>::max() / 3)
        throw std::invalid_argument("Ancho excesivo para el pitch RGB");
    if (options.samples > 4096 || options.max_bounces > 64)
        throw std::invalid_argument("Máximos: --samples 4096 y --bounces 64");
    if (options.model != ExecutionModel::coarse && (options.threads || !options.thread_counts.empty()))
        throw std::invalid_argument("--threads y --thread-counts requieren el modelo coarse");
    if (!options.thread_counts.empty() && !options.benchmark)
        throw std::invalid_argument("--thread-counts requiere --benchmark");
    if (options.benchmark) {
        options.headless = true;
        if (options.repetitions < 2) throw std::invalid_argument("Benchmark requiere al menos 2 repeticiones");
        if (!options.output.empty()) throw std::invalid_argument("Benchmark exporta CSV, no imágenes: usa --metrics-output");
        if (options.threads && !options.thread_counts.empty())
            throw std::invalid_argument("Usa --threads o --thread-counts, no ambos");
    }
    return options;
}
} // namespace rt
