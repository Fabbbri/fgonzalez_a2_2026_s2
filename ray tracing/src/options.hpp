#pragma once

#include <string>

namespace rt {
struct Options {
    int width = 640, height = 480, tile_width = 32, tile_height = 32;
    int samples = 128, max_bounces = 12;
    bool headless = false, help = false;
    std::string output;
};
// Interpreta y valida los argumentos; no inicializa escena ni ventana.
Options parse_options(int argc, char** argv);
} // namespace rt
