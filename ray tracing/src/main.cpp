#include "display.hpp"
#include "sequential.hpp"
#include "options.hpp"
#include "benchmark.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
using Clock = std::chrono::steady_clock; // Monotónico: no cambia al ajustar la hora.
using Duration = Clock::duration;
double milliseconds(Duration time) { return std::chrono::duration<double, std::milli>(time).count(); }
}

int main(int argc, char** argv) {
    try {
        const rt::Options options = rt::parse_options(argc, argv);
        if (options.help) {
            std::cout << "Uso: raytracer [--width W] [--height H] [--tile-size N]\n"
                         "                 [--tile-width W] [--tile-height H] [--headless]\n"
                         "                 [--samples N] [--bounces N] [--output imagen.ppm]\n"
                         "                 [--model sequential|fine|coarse|smt|cmp] [--quantum K]\n"
                         "                 [--block-interval-ms N] [--block-duration-ms N]\n"
                         "                 [--threads P] [--benchmark] [--repetitions N]\n"
                         "                 [--thread-counts 1,2,4,8] [--metrics-output archivo.csv]\n"
                         "                 [--cpu-info]\n"
                         "Por defecto: 640x480, tiles 32x32, 128 muestras, 12 rebotes.\n"
                         "Modelo sequential; fine: K=64; coarse: intervalo=10 ms, espera=1 ms.\n"
                         "Coarse crea un hilo por tile. Intervalo 0 desactiva bloqueos.\n"
                         "--threads fija P en coarse, SMT o CMP; --thread-counts permite barrer P.\n"
                         "CMP: un trabajador por núcleo físico. SMT: hilos lógicos o sobresuscripción.\n"
                         "SMT/CMP detectan CPU y usan afinidad Linux. Benchmark: headless, 200 repeticiones.\n"
                         "La escena se muestra al terminar. Esc cierra la ventana.\n";
            return 0;
        }
        rt::CpuTopology topology;
        if (options.cpu_info || (!options.benchmark &&
            (options.model == rt::ExecutionModel::smt || options.model == rt::ExecutionModel::cmp))) {
            topology = rt::detect_cpu_topology();
            std::cout << "nucleos_fisicos_disponibles=" << topology.cores.size()
                      << " cpus_logicas_disponibles=" << topology.logical_count()
                      << " smt_activo_linux=" << topology.smt_active << '\n';
            if (options.cpu_info) {
                for (std::size_t i = 0; i < topology.cores.size(); ++i)
                    std::cout << "nucleo=" << i << " cpus=" << rt::cpu_list(topology.cores[i]) << '\n';
                return 0;
            }
        }
        rt::Scene scene = rt::default_scene();
        scene.settings.samples = options.samples;
        scene.settings.max_bounces = options.max_bounces;
        // Cámara baja y frontal, próxima al suelo y dirigida hacia las esferas.
        const rt::Camera camera(options.width, options.height, {0, 0.45, 2.0}, {0, 0.25, -7.5});
        const rt::TileGrid tiles(options.width, options.height, options.tile_width, options.tile_height);
        if (options.benchmark) {
            rt::benchmark(options, scene, camera, tiles);
            return 0;
        }
        rt::Image image(options.width, options.height);
        std::unique_ptr<rt::Display> display;
        if (!options.headless) {
            display = std::make_unique<rt::Display>(options.width, options.height);
            // Fondo negro mientras se calcula; la textura no recibe tiles parciales.
            display->present(image);
        }
        rt::RenderHooks hooks;
        if (display) {
            hooks.keep_running = [&] { return display->poll(); };
            // Atender eventos no implica mostrar el progreso del buffer.
        }
        // PLANIFICACIÓN POR TILES: misma imagen y cálculo, distinta ejecución.
        // Los modelos con trabajadores hacen join antes de devolver.
        rt::RenderStats stats;
        switch (options.model) {
        case rt::ExecutionModel::sequential:
            stats = rt::render_sequential(scene, camera, image, tiles, hooks);
            break;
        case rt::ExecutionModel::fine:
            stats = rt::render_fine(scene, camera, image, tiles, options.quantum, hooks);
            break;
        case rt::ExecutionModel::coarse:
            stats = rt::render_coarse(scene, camera, image, tiles,
                                     options.block_interval_ms, options.block_duration_ms, hooks, options.threads);
            break;
        case rt::ExecutionModel::smt:
            stats = rt::render_smt(scene, camera, image, tiles, topology, options.threads, hooks);
            break;
        case rt::ExecutionModel::cmp:
            stats = rt::render_cmp(scene, camera, image, tiles, topology, options.threads, hooks);
            break;
        }
        const auto completed = stats.completed;
        const auto loop_time = stats.loop_time;
        const bool complete = completed == tiles.size();
        // Presentar solo cuando todos los tiles han terminado, fuera de la medición.
        if (complete && display) display->present(image);
        std::cout << std::fixed << std::setprecision(3)
                  << "estado=" << (complete ? "completo" : "cancelado")
                  << " modo=" << (options.headless ? "sin_ventana" : "ventana")
                  << " modelo=" << rt::model_name(options.model)
                  << " trabajadores=" << stats.workers
                  << " resolucion=" << options.width << 'x' << options.height
                  << " tile=" << options.tile_width << 'x' << options.tile_height
                  << " muestras=" << options.samples << " rebotes=" << options.max_bounces
                  << " tiles=" << completed << '/' << tiles.size() << '\n';
        if (options.model == rt::ExecutionModel::fine)
            std::cout << "K=" << options.quantum << " turnos=" << stats.quanta << '\n';
        if (options.model == rt::ExecutionModel::coarse)
            std::cout << "intervalo_bloqueo_ms=" << options.block_interval_ms
                      << " espera_bloqueo_ms=" << options.block_duration_ms
                      << " bloqueos=" << stats.blocks << '\n';
        if (options.model == rt::ExecutionModel::smt || options.model == rt::ExecutionModel::cmp) {
            const auto cpus = options.model == rt::ExecutionModel::cmp
                ? rt::cmp_cpus(topology, tiles.size(), options.threads)
                : rt::smt_cpus(topology, tiles.size(), options.threads);
            std::cout << "afinidad_cpus=" << rt::cpu_list(cpus) << '\n';
            if (options.model == rt::ExecutionModel::smt && cpus.size() <= topology.cores.size())
                std::cout << "SMT: P <= C; esta configuración no sobresuscribe núcleos físicos.\n";
        }
        std::cout << "tiempo_total_ms=" << milliseconds(loop_time) << '\n';
        // Nunca etiquetar una imagen parcial como resultado completo.
        if (complete && !options.output.empty()) {
            const auto start = Clock::now();
            image.save_ppm(options.output);
            std::cout << "escritura_ms=" << milliseconds(Clock::now() - start)
                      << " archivo=" << options.output << '\n';
        } else if (!complete && !options.output.empty()) {
            std::cout << "Salida omitida: renderizado cancelado.\n";
        }
        std::cout << std::flush;
        if (complete && display) display->wait_until_closed();
        return complete ? 0 : 2;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
