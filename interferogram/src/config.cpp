#include "interferogram/config.hpp"
#include "interferogram/slc_reader.hpp"
#include <charconv>
#include <ostream>
#include <stdexcept>
#include <string>

namespace interferogram {
namespace {
int integer(const std::string& text, const std::string& option) {
    int value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size())
        throw std::invalid_argument("Valor entero inválido para " + option);
    return value;
}
}
Config parseConfig(int argc, char* argv[]) {
    Config config;
    for (int i = 1; i < argc; ++i) {
        const std::string option = argv[i];
        if (option == "--help") {
            config.help = true;
            continue;
        }
        if (option != "--a" && option != "--b" && option != "--width" &&
            option != "--height" && option != "--x" && option != "--y")
            throw std::invalid_argument("Opción desconocida: " + option);
        if (++i == argc || std::string(argv[i]).empty() ||
            std::string(argv[i]).rfind("--", 0) == 0)
            throw std::invalid_argument("Falta valor para " + option);
        const std::string value = argv[i];
        if (option == "--a") config.a = value;
        else if (option == "--b") config.b = value;
        else if (option == "--width") config.width = integer(value, option);
        else if (option == "--height") config.height = integer(value, option);
        else if (option == "--x") config.x = integer(value, option);
        else if (option == "--y") config.y = integer(value, option);
    }
    if (!config.help)
        validateWindow(config.x, config.y, config.width, config.height);
    return config;
}
void printHelp(std::ostream& out) {
    out << "Uso: ./build/interferogram [opciones]\n"
        << "Kernel secuencial A * conjugate(B) sobre SLC complejos P09-R2.\n"
        << "  --a PATH       TIFF A (predeterminado: 2025-01-10, burst 179298)\n"
        << "  --b PATH       TIFF B (predeterminado: 2025-01-22, burst 179298)\n"
        << "  --width N      Ancho de ventana (512)\n"
        << "  --height N     Alto de ventana (512)\n"
        << "  --x N          Columna inicial, base cero (0)\n"
        << "  --y N          Fila inicial, base cero (0)\n"
        << "  --help         Mostrar ayuda\n"
        << "Misma ventana para A/B, contenida en 24445 x 1500.\n"
        << "Ejecutar desde la raíz del repositorio para usar las rutas predeterminadas.\n";
}
} // namespace interferogram
