/**
 * @file options.cpp
 * Análisis y validación de argumentos de línea de comandos.
 * Convierte texto en Options sin construir escena, ventana ni trabajadores.
 * Las restricciones evitan combinaciones incompatibles entre modelos y modos.
 */
#include "app/options.hpp"
#include <charconv>
#include <limits>
#include <stdexcept>

namespace rt {
namespace {
// Convierte un argumento completo a entero. allow_zero solo habilita cero
// para duraciones e intervalos donde ese valor tiene semántica definida.
int integer(const std::string& value, bool allow_zero = false) {
    int result = 0;
    // from_chars no depende de configuración regional y señala rango y fin de lectura.
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    // Rechaza conversión fallida, sufijos sin consumir y valores fuera del dominio.
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size() ||
        result < 0 || (!allow_zero && result == 0))
        throw std::invalid_argument("Entero fuera de rango: " + value);
    return result;
}
} // namespace
Options parse_options(int argc, char** argv) {
    // Parte de los valores predeterminados definidos en options.hpp.
    Options options;
    // Recorre argv omitiendo argv[0], que contiene el nombre del ejecutable.
    for (int i = 1; i < argc; ++i) {
        // Copia la bandera actual para comparar su nombre de forma segura.
        const std::string flag = argv[i];
        // Banderas sin valor: activan modos y no consumen otro argumento.
        if (flag == "--headless") options.headless = true;
        else if (flag == "--cpu-info") options.cpu_info = true;
        else if (flag == "--benchmark") options.benchmark = true;
        else if (flag == "--help" || flag == "-h") options.help = true;
        else {
            // Lista cerrada de opciones con valor; un nombre desconocido se rechaza.
            if (flag != "--width" && flag != "--height" && flag != "--tile-width" &&
                flag != "--tile-height" && flag != "--tile-size" && flag != "--output" &&
                flag != "--samples" && flag != "--bounces" && flag != "--model" &&
                flag != "--quantum" && flag != "--block-interval-ms" && flag != "--block-duration-ms" &&
                flag != "--threads" && flag != "--thread-counts" && flag != "--repetitions" && flag != "--metrics-output" && flag != "--baseline-csv")
                throw std::invalid_argument("Opción desconocida: " + flag);
            // Consume el valor asociado y detecta una opción al final sin argumento.
            if (++i == argc) throw std::invalid_argument("Falta valor para " + flag);
            const std::string value = argv[i];
            // Traduce etiquetas textuales a la enumeración ExecutionModel.
            if (flag == "--model") {
                if (value == "sequential") options.model = ExecutionModel::sequential;
                else if (value == "fine") options.model = ExecutionModel::fine;
                else if (value == "coarse") options.model = ExecutionModel::coarse;
                else if (value == "smt") options.model = ExecutionModel::smt;
                else if (value == "cmp") options.model = ExecutionModel::cmp;
                else throw std::invalid_argument("Modelos: sequential, fine, coarse, smt, cmp");
            } else if (flag == "--thread-counts") {
                // La última lista sustituye la anterior, sin acumular barridos duplicados.
                options.thread_counts.clear();
                // Posición inicial del siguiente entero de la lista separada por comas.
                std::size_t begin = 0;
                do {
                    // Busca el delimitador; npos identifica el último elemento.
                    const auto end = value.find(',', begin);
                    // Cada subcadena pasa por la misma validación de entero positivo.
                    options.thread_counts.push_back(integer(value.substr(begin, end - begin)));
                    if (end == std::string::npos) break;
                    begin = end + 1;
                } while (true);
            } else if (flag == "--baseline-csv") {
                if (value.empty()) throw std::invalid_argument("Ruta de referencia vacía");
                options.baseline_csv = value;
            } else if (flag == "--metrics-output") {
                if (value.empty()) throw std::invalid_argument("Ruta CSV vacía");
                options.metrics_output = value;
            } else if (flag == "--output") {
                if (value.empty()) throw std::invalid_argument("Archivo de salida vacío");
                options.output = value;
            } else {
                // Conversión común de valores numéricos; cero solo en bloqueos.
                const int n = integer(value, flag == "--block-interval-ms" || flag == "--block-duration-ms");
                // Asigna cada parámetro a Options. --tile-size modifica ambas dimensiones;
                // al recorrer en orden, la última aparición prevalece.
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
    // RGB24 usa tres bytes por píxel: limita el producto del pitch dentro de int.
    if (options.width > std::numeric_limits<int>::max() / 3)
        throw std::invalid_argument("Ancho excesivo para el pitch RGB");
    // Limita la carga máxima admitida de integración por píxel.
    if (options.samples > 4096 || options.max_bounces > 64)
        throw std::invalid_argument("Máximos: --samples 4096 y --bounces 64");
    // Sequential y fine usan un hilo real y no admiten configurar P.
    if ((options.model == ExecutionModel::sequential || options.model == ExecutionModel::fine) &&
        (options.threads || !options.thread_counts.empty()))
        throw std::invalid_argument("--threads y --thread-counts requieren coarse, smt o cmp");
    // El barrido de trabajadores solo tiene significado en modo benchmark.
    if (!options.thread_counts.empty() && !options.benchmark)
        throw std::invalid_argument("--thread-counts requiere --benchmark");
    // Una referencia importada corresponde a una medición de otro modelo.
    if (!options.baseline_csv.empty() && (!options.benchmark || options.model == ExecutionModel::sequential))
        throw std::invalid_argument("--baseline-csv requiere benchmark de un modelo distinto de sequential");
    // Impone modo sin ventana y condiciones necesarias para estadística muestral.
    if (options.benchmark) {
        // Evita que eventos y presentación contaminen la campaña.
        options.headless = true;
        if (options.repetitions < 2) throw std::invalid_argument("Benchmark requiere al menos 2 repeticiones");
        if (!options.output.empty()) throw std::invalid_argument("Benchmark exporta CSV, no imágenes: usa --metrics-output");
        if (options.threads && !options.thread_counts.empty())
            throw std::invalid_argument("Usa --threads o --thread-counts, no ambos");
    }
    return options;
}
} // namespace rt
