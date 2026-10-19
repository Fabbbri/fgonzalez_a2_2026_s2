/**
 * @file benchmark.cpp
 * Campaña reproducible con calentamiento y validación de imagen.
 * Alterna el orden referencia/modelo al medir pares; también puede importar
 * una referencia secuencial compartida. Las mediciones usan loop_time del modelo.
 * Hash, comparación de imagen y escritura de CSV se realizan después del render.
 */
#include "metrics/benchmark.hpp"
#include "metrics/metrics.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <map>
#include <cstdint>
#include <cmath>
#include <numeric>
#include <stdexcept>

namespace rt {
namespace {
// Identificar la imagen fuera del intervalo cronometrado permite comprobarla
// contra la referencia guardada sin ejecutar de nuevo sequential.
std::string image_hash(const Image& image, const Options& options) {
    // Base de FNV-1a de 64 bits; su desbordamiento sin signo es parte del algoritmo.
    std::uint64_t hash = 14695981039346656037ull;
    // Vista de bytes de la imagen RGB, sin copiar el buffer.
    const auto* bytes = reinterpret_cast<const unsigned char*>(image.data());
    // Recorre los 3*W*H bytes; i es índice de byte y no participa en el paralelismo.
    for (std::size_t i = 0; i < static_cast<std::size_t>(options.width) * options.height * 3; ++i) {
        // Incorpora el byte mediante XOR antes de multiplicar por el primo FNV.
        hash ^= bytes[i];
        hash *= 1099511628211ull;
    }
    // Stream en memoria para expresar la huella en hexadecimal.
    std::ostringstream out;
    out << std::hex << hash;
    return out.str();
}
// Referencia importada: conserva serie original y procedencia de imagen y SMT.
struct Baseline {
    std::vector<double> times;
    std::string hash, smt;
};
// Carga una referencia sequential y verifica compatibilidad experimental.
Baseline read_baseline(const Options& options, std::uint64_t seed) {
    std::ifstream input(options.baseline_csv);
    if (!input) throw std::runtime_error("No se pudo abrir la referencia sequential");
    // El CSV sequential no contiene rutas ni comas entrecomilladas.
    // Separador para el esquema de referencia simple, cuyas columnas no contienen comas escapadas.
    const auto split = [](const std::string& line) {
        std::vector<std::string> cells;
        std::istringstream stream(line);
        std::string cell;
        while (std::getline(stream, cell, ',')) cells.push_back(cell);
        return cells;
    };
    std::string line;
    std::getline(input, line);
    // Cabecera del CSV que permite localizar columnas por nombre, no por orden fijo.
    const auto header = split(line);
    // Tabla nombre -> posición para resolver cada campo de las filas.
    std::map<std::string, std::size_t> columns;
    for (std::size_t i = 0; i < header.size(); ++i) columns[header[i]] = i;
    Baseline result;
    // Procesa ejecuciones de referencia; las filas vacías se omiten.
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        const auto cells = split(line);
        // Acceso validado a una columna: referencia a la celda, sin copiar su contenido.
        const auto field = [&](const std::string& key) -> const std::string& {
            const auto at = columns.find(key);
            if (at == columns.end() || at->second >= cells.size())
                throw std::invalid_argument("Referencia sin columna " + key);
            return cells[at->second];
        };
        // Exige modelo secuencial de un hilo y coincidencia de resolución, tiles,
        // muestras, rebotes y semilla con la carga actual.
        if (field("modelo") != "sequential" || field("hilos") != "1" ||
            field("ancho") != std::to_string(options.width) || field("alto") != std::to_string(options.height) ||
            field("tile_ancho") != std::to_string(options.tile_width) || field("tile_alto") != std::to_string(options.tile_height) ||
            field("muestras") != std::to_string(options.samples) || field("rebotes") != std::to_string(options.max_bounces) ||
            field("semilla") != std::to_string(seed))
            throw std::invalid_argument("La referencia no corresponde a sequential con la misma carga");
        // Impide combinar imágenes o estados SMT distintos dentro de una referencia.
        if (!result.times.empty() && (field("imagen_hash") != result.hash || field("referencia_smt") != result.smt))
            throw std::invalid_argument("La referencia mezcla imágenes o estados SMT");
        result.hash = field("imagen_hash");
        result.smt = field("referencia_smt");
        const auto& value = field("tiempo_total_ms");
        std::size_t used = 0;
        // Lee tiempo y verifica consumo completo, finitud y positividad.
        const double time = std::stod(value, &used);
        if (used != value.size() || !std::isfinite(time) || time <= 0 || result.hash.empty())
            throw std::invalid_argument("Referencia con tiempo o imagen inválidos");
        result.times.push_back(time);
    }
    if (result.times.size() < 2) throw std::invalid_argument("Referencia requiere al menos 2 ejecuciones");
    return result;
}
// Escapa texto CSV entre comillas; duplica comillas internas según la convención.
std::string csv_string(const std::string& value) {
    std::string result = "\"";
    for (char c : value) { result += c; if (c == '"') result += c; }
    return result + '"';
}
} // namespace
void benchmark(const Options& options, const Scene& scene, const Camera& camera,
               const TileGrid& tiles) {
    // Selecciona los modelos que registran topología y destinos de afinidad.
    const bool hardware_model = options.model == ExecutionModel::smt || options.model == ExecutionModel::cmp;
    const auto topology = detect_cpu_topology();
    // Distingue referencia importada de comparación pareada medida en esta campaña.
    const bool shared = !options.baseline_csv.empty();
    const auto baseline = shared ? read_baseline(options, scene.settings.seed) : Baseline{};
    // Media única de la referencia compartida; se usa para cocientes descriptivos por fila.
    const double baseline_mean = shared ? std::accumulate(baseline.times.begin(), baseline.times.end(), 0.0) / baseline.times.size() : 0;
    // Configuraciones de P solicitadas; vacío implica usar la selección individual.
    std::vector<int> counts = options.thread_counts;
    if (counts.empty()) counts.push_back(options.threads);
    // Ordena y elimina duplicados para medir una sola vez cada P solicitado.
    std::sort(counts.begin(), counts.end());
    counts.erase(std::unique(counts.begin(), counts.end()), counts.end());
    for (int count : counts) if (static_cast<std::size_t>(count) > tiles.size())
        throw std::invalid_argument("El número de hilos no puede superar el número de tiles");
    // Validar todas las configuraciones antes de sobrescribir archivos o medir.
    for (int count : counts) {
        if (options.model == ExecutionModel::cmp) cmp_cpus(topology, tiles.size(), count);
        if (options.model == ExecutionModel::smt) smt_cpus(topology, tiles.size(), count);
    }

    // Ruta del archivo por repetición; su carpeta se crea si no existe.
    const std::filesystem::path raw_path(options.metrics_output);
    if (!raw_path.parent_path().empty()) std::filesystem::create_directories(raw_path.parent_path());
    // Archivo asociado con sufijo -summary, una fila por configuración.
    const auto summary_path = raw_path.parent_path() / (raw_path.stem().string() + "-summary.csv");
    // Protege la referencia de sobrescritura por cualquiera de los archivos de salida.
    if (shared && (std::filesystem::absolute(options.baseline_csv).lexically_normal() == std::filesystem::absolute(raw_path).lexically_normal() ||
                   std::filesystem::absolute(options.baseline_csv).lexically_normal() == std::filesystem::absolute(summary_path).lexically_normal()))
        throw std::invalid_argument("La salida no puede sobrescribir la referencia");
    // Abre ambos archivos antes de comenzar las mediciones.
    std::ofstream raw(raw_path), summary(summary_path);
    if (!raw || !summary) throw std::runtime_error("No se pudieron abrir los CSV de métricas");
    // Conserva doce dígitos significativos al serializar tiempos y cocientes.
    raw << std::setprecision(12);
    summary << std::setprecision(12);
    // Esquema del CSV crudo: carga, política, tiempos, topología y procedencia de referencia.
    raw << "modelo,hilos,repeticion,ancho,alto,tile_ancho,tile_alto,muestras,rebotes,K,intervalo_ms,espera_ms,semilla,tiempo_total_ms,tiempo_secuencial_ms,speedup,eficiencia_paralela,nucleos_fisicos_disponibles,cpus_logicas_disponibles,smt_activo_linux,afinidad_cpus,referencia_tipo,referencia_csv,imagen_hash,referencia_smt\n";
    // Esquema agregado: medias, dispersión e intervalos de tiempo, S y E.
    summary << "modelo,hilos,repeticiones,tiempo_total_media_ms,tiempo_total_desviacion_ms,tiempo_total_ic95_aprox_inferior,tiempo_total_ic95_aprox_superior,tiempo_secuencial_media_ms,speedup_media,speedup_desviacion,speedup_ic95_aprox_inferior,speedup_ic95_aprox_superior,eficiencia_media,eficiencia_ic95_aprox_inferior,eficiencia_ic95_aprox_superior\n";
    // Buffers de tamaño fijo construidos antes del cronómetro de los modelos.
    rt::Image reference(options.width, options.height), image(options.width, options.height);
    // Despachador del modelo medido con el P actual; todos escriben la misma image.
    // El bucle de configuraciones es secuencial, sin campañas compitiendo entre sí.
    const auto run = [&](int threads) {
        switch (options.model) {
        case ExecutionModel::sequential: return render_sequential(scene, camera, image, tiles);
        case ExecutionModel::fine: return render_fine(scene, camera, image, tiles, options.quantum);
        case ExecutionModel::coarse:
            return render_coarse(scene, camera, image, tiles, options.block_interval_ms,
                                 options.block_duration_ms, {}, threads);
        case ExecutionModel::smt: return render_smt(scene, camera, image, tiles, topology, threads);
        case ExecutionModel::cmp: return render_cmp(scene, camera, image, tiles, topology, threads);
        }
        throw std::invalid_argument("Modelo inválido");
    };
    // Convierte loop_time a milisegundos; incluye toda la ejecución del modelo.
    const auto ms = [](const RenderStats& stats) {
        return std::chrono::duration<double, std::milli>(stats.loop_time).count();
    };
    std::cout << "metricas: tiempo total del modelo, speedup, eficiencia y escalabilidad por hilos\n";
    std::cout << "escalabilidad=" << (counts.size() > 1 ? "barrido_de_hilos" : "una_configuracion") << '\n';
    if (shared) std::cout << "referencia_compartida=" << options.baseline_csv << " n=" << baseline.times.size()
                         << " media_ms=" << baseline_mean << " smt=" << baseline.smt << '\n';
    if (options.repetitions < 200)
        std::cout << "repeticiones=" << options.repetitions << ": prueba corta; referencia del proyecto=200\n";
    // Barrido de P manteniendo constantes escena, resolución y cantidad M de tiles.
    for (int threads : counts) {
        // Lista de afinidad para registrar destinos; los otros modelos no fijan CPU.
        const auto cpus = options.model == ExecutionModel::cmp ? cmp_cpus(topology, tiles.size(), threads)
            : options.model == ExecutionModel::smt ? smt_cpus(topology, tiles.size(), threads) : std::vector<int>{};
        if (hardware_model) {
            std::cout << "nucleos_fisicos_disponibles=" << topology.cores.size()
                      << " cpus_logicas_disponibles=" << topology.logical_count()
                      << " smt_activo_linux=" << topology.smt_active
                      << " afinidad_cpus=" << cpu_list(cpus) << std::endl;
            if (options.model == ExecutionModel::smt && cpus.size() <= topology.cores.size())
                std::cout << "SMT: P <= C; sin sobresuscripción de núcleos físicos en esta configuración.\n";
        }
        // Calentamiento fuera de las mediciones exportadas.
        if (!shared && options.model != ExecutionModel::sequential)
            render_sequential(scene, camera, reference, tiles);
        // Calentamiento del modelo que no se incorpora a samples ni al CSV.
        run(threads);
        if (shared && image_hash(image, options) != baseline.hash)
            throw std::runtime_error("La imagen difiere de la referencia compartida");
        // Colección de repeticiones válidas de esta configuración, reiniciada para cada P.
        std::vector<Measurement> samples;
        std::size_t workers = 1;
        // Repite medición y verificación antes de exportar cada observación.
        for (int repetition = 0; repetition < options.repetitions; ++repetition) {
            RenderStats base, measured;
            // Con referencia importada solo renderiza el modelo; no vuelve a medir sequential.
            if (shared) {
                measured = run(threads);
                base.completed = tiles.size(); // No ejecutar sequential: sus tiempos ya están en CSV.
            // Al medir la base, usa el mismo tiempo como numerador y denominador: S=1.
            } else if (options.model == ExecutionModel::sequential) {
                measured = run(threads);
                base = measured; // La referencia contra sí misma tiene speedup 1.
            // Alterna orden secuencial/modelo entre repeticiones para reducir sesgo de orden.
            } else if (repetition % 2 == 0) {
                base = render_sequential(scene, camera, reference, tiles);
                measured = run(threads);
            } else {
                measured = run(threads);
                base = render_sequential(scene, camera, reference, tiles);
            }
            // Descarta cualquier medición en la que no terminaron los M tiles.
            if (measured.completed != tiles.size() || base.completed != tiles.size())
                throw std::runtime_error("Medición incompleta");
            // Comparación exacta por bytes fuera del intervalo cronometrado del render.
            if (!shared && options.model != ExecutionModel::sequential &&
                std::memcmp(reference.data(), image.data(), static_cast<std::size_t>(options.width) * options.height * 3))
                throw std::runtime_error("El modelo cambió la imagen: no se exportan métricas válidas");
            // Usa P realmente creado, no el centinela automático ni la cantidad de tareas fine.
            workers = measured.workers;
            const auto hash = image_hash(image, options);
            if (shared && hash != baseline.hash)
                throw std::runtime_error("El modelo cambió la imagen de la referencia compartida");
            // Separa tiempo del modelo y referencia según método de comparación.
            const double total = ms(measured), sequential = shared ? baseline_mean : ms(base);
            if (total <= 0 || sequential <= 0) throw std::runtime_error("Resolución insuficiente del reloj");
            // Conserva tiempos válidos para estadística posterior a las repeticiones.
            samples.push_back({sequential, total});
            const double speedup = sequential / total;
            // Una fila por ejecución. K y bloqueos no aplican a SMT/CMP; procedencia
            // y huella permiten reconstruir la comparación con su referencia.
            raw << model_name(options.model) << ',' << workers << ',' << repetition + 1 << ','
                << options.width << ',' << options.height << ',' << options.tile_width << ',' << options.tile_height << ','
                << options.samples << ',' << options.max_bounces << ',' << (hardware_model ? 0 : options.quantum) << ','
                << (hardware_model ? 0 : options.block_interval_ms) << ','
                << (hardware_model ? 0 : options.block_duration_ms) << ',' << scene.settings.seed << ','
                << total << ',' << sequential << ',' << speedup << ',' << speedup / workers << ','
                << (hardware_model ? std::to_string(topology.cores.size()) : "no_aplica") << ','
                << (hardware_model ? std::to_string(topology.logical_count()) : "no_aplica") << ','
                << (hardware_model ? topology.smt_active : "no_aplica") << ','
                << (hardware_model ? cpu_list(cpus) : "no_aplica") << ','
                << (shared ? "compartida" : options.model == ExecutionModel::sequential ? "propia" : "pareada") << ','
                << csv_string(shared ? std::filesystem::relative(std::filesystem::absolute(options.baseline_csv),
                    std::filesystem::absolute(raw_path).parent_path()).string() : "") << ','
                << hash << ',' << (shared ? baseline.smt : topology.smt_active) << '\n';
        }
        // Selecciona promedio pareado o razón de medias con propagación delta.
        const auto result = shared ? summarize_shared(samples, baseline.times, workers) : summarize(samples, workers);
        // Exporta el agregado una vez reunidas todas las observaciones del P actual.
        summary << model_name(options.model) << ',' << workers << ',' << samples.size() << ','
                << result.total.mean << ',' << result.total.deviation << ',' << result.total.ci_low << ',' << result.total.ci_high << ','
                << result.sequential.mean << ',' << result.speedup.mean << ',' << result.speedup.deviation << ','
                << result.speedup.ci_low << ',' << result.speedup.ci_high << ',' << result.efficiency.mean << ','
                << result.efficiency.ci_low << ',' << result.efficiency.ci_high << '\n';
        std::cout << std::fixed << std::setprecision(4)
                  << "modelo=" << model_name(options.model) << " hilos=" << workers
                  << " repeticiones=" << samples.size() << " tiempo_total_media_ms=" << result.total.mean
                  << " speedup=" << result.speedup.mean << " eficiencia_paralela=" << result.efficiency.mean
                  << " eficiencia_porcentaje=" << result.efficiency.mean * 100 << '\n';
    }
    // Vacía y cierra salidas, comprobando después errores de E/S tardíos.
    raw.close(); summary.close();
    if (!raw || !summary) throw std::runtime_error("Error al escribir los CSV de métricas");
    std::cout << "mediciones_csv=" << raw_path.string() << " resumen_csv=" << summary_path.string() << '\n';
}
} // namespace rt
