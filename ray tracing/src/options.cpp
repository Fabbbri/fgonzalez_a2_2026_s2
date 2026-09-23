#include "options.hpp"
#include <charconv>
#include <limits>
#include <stdexcept>

namespace rt {
namespace {
int positive(const std::string& value) {
    int result = 0;
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() || result <= 0)
        throw std::invalid_argument("Se esperaba un entero positivo: " + value);
    return result;
}
} // namespace
Options parse_options(int argc, char** argv) {
    Options options;
    for (int i = 1; i < argc; ++i) {
        const std::string flag = argv[i];
        if (flag == "--headless") options.headless = true;
        else if (flag == "--help" || flag == "-h") options.help = true;
        else {
            if (flag != "--width" && flag != "--height" && flag != "--tile-width" &&
                flag != "--tile-height" && flag != "--tile-size" && flag != "--output" &&
                flag != "--samples" && flag != "--bounces")
                throw std::invalid_argument("Opción desconocida: " + flag);
            if (++i == argc) throw std::invalid_argument("Falta valor para " + flag);
            const std::string value = argv[i];
            if (flag == "--output") {
                if (value.empty()) throw std::invalid_argument("Archivo de salida vacío");
                options.output = value;
            } else {
                const int n = positive(value);
                if (flag == "--width") options.width = n;
                if (flag == "--height") options.height = n;
                if (flag == "--samples") options.samples = n;
                if (flag == "--bounces") options.max_bounces = n;
                if (flag == "--tile-width" || flag == "--tile-size") options.tile_width = n;
                if (flag == "--tile-height" || flag == "--tile-size") options.tile_height = n;
            }
        }
    }
    if (options.width > std::numeric_limits<int>::max() / 3)
        throw std::invalid_argument("Ancho excesivo para el pitch RGB");
    if (options.samples > 4096 || options.max_bounces > 64)
        throw std::invalid_argument("Máximos: --samples 4096 y --bounces 64");
    return options;
}
} // namespace rt
