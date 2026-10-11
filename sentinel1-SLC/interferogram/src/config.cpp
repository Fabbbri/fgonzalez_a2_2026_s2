#include "interferogram/config.hpp"
#include "interferogram/slc_reader.hpp"

#include <charconv>
#include <ostream>
#include <stdexcept>
#include <string>

namespace interferogram {

namespace {

/*
 * Convierte una cadena de texto en un valor entero.
 *
 * @param text   Cadena que contiene el valor por convertir.
 * @param option Nombre de la opción de la CLI asociada al valor.
 * @return El valor entero convertido.
 *
 * @throws std::invalid_argument si el valor no representa un entero válido,
 *         está fuera del rango admitido o contiene caracteres adicionales.
 */
int integer(const std::string& text, const std::string& option) {
    int value = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);

    if (parsed.ec != std::errc{} || parsed.ptr != text.data() + text.size()) {
        throw std::invalid_argument("Valor entero inválido para " + option);
    }

    return value;
}

} // namespace

/*
 * Construye la configuración del programa a partir de los argumentos
 * recibidos por la línea de comandos.
 *
 * @param argc Número de argumentos recibidos.
 * @param argv Arreglo de argumentos de la línea de comandos.
 * @return Configuración resultante.
 *
 * @throws std::invalid_argument ante una opción desconocida, un valor
 *         ausente, un entero inválido o una ventana fuera de los límites.
 */
Config parseConfig(int argc, char* argv[]) {
    Config config;

    for (int i = 1; i < argc; ++i) {
        const std::string option = argv[i];

        if (option == "--help") {
            config.help = true;
            continue;
        }

        if (option != "--a" && option != "--b" && option != "--width" && option != "--height" &&
            option != "--x" && option != "--y" && option != "--mode" &&
            option != "--tile-width" && option != "--tile-height" && option != "--quantum" && option != "--threads") {
            throw std::invalid_argument("Opción desconocida: " + option);
        }

        // Verifica que la opción tenga un valor asociado antes de consumirlo.
        if (++i == argc || std::string(argv[i]).empty() || std::string(argv[i]).rfind("--", 0) == 0) {
            throw std::invalid_argument("Falta valor para " + option);
        }

        const std::string value = argv[i];

        if (option == "--threads")
            config.threads = integer(value, option);
        else if (option == "--quantum")
            config.quantum = integer(value, option);
        else if (option == "--mode")
            config.mode = value;
        else if (option == "--tile-width")
            config.tileWidth = integer(value, option);
        else if (option == "--tile-height")
            config.tileHeight = integer(value, option);
        else if (option == "--a")
            config.a = value;
        else if (option == "--b")
            config.b = value;
        else if (option == "--width")
            config.width = integer(value, option);
        else if (option == "--height")
            config.height = integer(value, option);
        else if (option == "--x")
            config.x = integer(value, option);
        else if (option == "--y")
            config.y = integer(value, option);
    }

    if (!config.help) {
        validateWindow(config.x, config.y, config.width, config.height);
        if (config.mode != "sequential" && config.mode != "tiled" && config.mode != "fine-grained" && config.mode != "coarse-grained")
            throw std::invalid_argument("Modo válido: sequential, tiled, fine-grained o coarse-grained.");
        if (config.threads <= 0)
            throw std::invalid_argument("La cantidad de hilos debe ser positiva.");
        if (config.quantum <= 0)
            throw std::invalid_argument("El quantum debe ser positivo.");
        if (config.tileWidth <= 0 || config.tileHeight <= 0)
            throw std::invalid_argument("El tamaño del tile debe ser positivo.");
    }

    return config;
}

/*
 * Escribe en el flujo indicado la ayuda de uso de la interfaz
 * de línea de comandos.
 *
 * La función únicamente genera el texto informativo; no abre archivos SLC
 * ni ejecuta el cálculo del interferograma.
 *
 * @param out Flujo de salida donde se escribe la ayuda.
 */
void printHelp(std::ostream& out) {
    out << "Uso: ./build/interferogram [opciones]\n"
        << "Kernel secuencial A * conjugate(B) sobre SLC complejos P09-R2.\n"
        << "  --a PATH       TIFF A (predeterminado: 2025-01-10, burst 179298)\n"
        << "  --b PATH       TIFF B (predeterminado: 2025-01-22, burst 179298)\n"
        << "  --width N      Ancho de ventana (512)\n"
        << "  --height N     Alto de ventana (512)\n"
        << "  --x N          Columna inicial, base cero (0)\n"
        << "  --y N          Fila inicial, base cero (0)\n"
        << "  --mode MODE    sequential, tiled, fine-grained o coarse-grained (sequential)\n"
        << "  --tile-width N Ancho del tile (32)\n"
        << "  --tile-height N Alto del tile (32)\n"
        << "  --quantum N    Píxeles por turno en fine-grained (1)\n"
        << "  --threads N    Trabajadores coarse-grained (2)\n"
        << "  --help         Mostrar ayuda\n"
        << "Misma ventana para A/B, contenida en las dimensiones de cada TIFF.\n"
        << "Ejecutar desde sentinel1-SLC/ para usar las rutas predeterminadas.\n";
}

} 
