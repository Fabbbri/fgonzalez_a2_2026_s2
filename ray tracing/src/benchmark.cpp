#include "benchmark.hpp"
#include "metrics.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace rt {
void benchmark(const Options& options, const Scene& scene, const Camera& camera,
               const TileGrid& tiles) {
    const bool hardware_model = options.model == ExecutionModel::smt || options.model == ExecutionModel::cmp;
    const auto topology = hardware_model ? detect_cpu_topology() : CpuTopology{};
    std::vector<int> counts = options.thread_counts;
    if (counts.empty()) counts.push_back(options.threads);
    std::sort(counts.begin(), counts.end());
    counts.erase(std::unique(counts.begin(), counts.end()), counts.end());
    for (int count : counts) if (static_cast<std::size_t>(count) > tiles.size())
        throw std::invalid_argument("El número de hilos no puede superar el número de tiles");
    // Validar todas las configuraciones antes de sobrescribir archivos o medir.
    for (int count : counts) {
        if (options.model == ExecutionModel::cmp) cmp_cpus(topology, tiles.size(), count);
        if (options.model == ExecutionModel::smt) smt_cpus(topology, tiles.size(), count);
    }

    const std::filesystem::path raw_path(options.metrics_output);
    if (!raw_path.parent_path().empty()) std::filesystem::create_directories(raw_path.parent_path());
    const auto summary_path = raw_path.parent_path() / (raw_path.stem().string() + "-summary.csv");
    std::ofstream raw(raw_path), summary(summary_path);
    if (!raw || !summary) throw std::runtime_error("No se pudieron abrir los CSV de métricas");
    raw << std::setprecision(12);
    summary << std::setprecision(12);
    raw << "modelo,hilos,repeticion,ancho,alto,tile_ancho,tile_alto,muestras,rebotes,K,intervalo_ms,espera_ms,semilla,tiempo_total_ms,tiempo_secuencial_ms,speedup,eficiencia_paralela,nucleos_fisicos_disponibles,cpus_logicas_disponibles,smt_activo_linux,afinidad_cpus\n";
    summary << "modelo,hilos,repeticiones,tiempo_total_media_ms,tiempo_total_desviacion_ms,tiempo_total_ic95_aprox_inferior,tiempo_total_ic95_aprox_superior,tiempo_secuencial_media_ms,speedup_media,speedup_desviacion,speedup_ic95_aprox_inferior,speedup_ic95_aprox_superior,eficiencia_media,eficiencia_ic95_aprox_inferior,eficiencia_ic95_aprox_superior\n";
    rt::Image reference(options.width, options.height), image(options.width, options.height);
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
    const auto ms = [](const RenderStats& stats) {
        return std::chrono::duration<double, std::milli>(stats.loop_time).count();
    };
    std::cout << "metricas: tiempo total del modelo, speedup, eficiencia y escalabilidad por hilos\n";
    std::cout << "escalabilidad=" << (counts.size() > 1 ? "barrido_de_hilos" : "una_configuracion") << '\n';
    if (options.repetitions < 200)
        std::cout << "repeticiones=" << options.repetitions << ": prueba corta; referencia del proyecto=200\n";
    for (int threads : counts) {
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
        render_sequential(scene, camera, reference, tiles);
        run(threads);
        std::vector<Measurement> samples;
        std::size_t workers = 1;
        for (int repetition = 0; repetition < options.repetitions; ++repetition) {
            RenderStats base, measured;
            if (options.model == ExecutionModel::sequential) {
                measured = run(threads);
                base = measured; // La referencia contra sí misma tiene speedup 1.
            } else if (repetition % 2 == 0) {
                base = render_sequential(scene, camera, reference, tiles);
                measured = run(threads);
            } else {
                measured = run(threads);
                base = render_sequential(scene, camera, reference, tiles);
            }
            if (measured.completed != tiles.size() || base.completed != tiles.size())
                throw std::runtime_error("Medición incompleta");
            if (options.model != ExecutionModel::sequential &&
                std::memcmp(reference.data(), image.data(), static_cast<std::size_t>(options.width) * options.height * 3))
                throw std::runtime_error("El modelo cambió la imagen: no se exportan métricas válidas");
            workers = measured.workers;
            const double total = ms(measured), sequential = ms(base);
            if (total <= 0 || sequential <= 0) throw std::runtime_error("Resolución insuficiente del reloj");
            samples.push_back({sequential, total});
            const double speedup = sequential / total;
            raw << model_name(options.model) << ',' << workers << ',' << repetition + 1 << ','
                << options.width << ',' << options.height << ',' << options.tile_width << ',' << options.tile_height << ','
                << options.samples << ',' << options.max_bounces << ',' << (hardware_model ? 0 : options.quantum) << ','
                << (hardware_model ? 0 : options.block_interval_ms) << ','
                << (hardware_model ? 0 : options.block_duration_ms) << ',' << scene.settings.seed << ','
                << total << ',' << sequential << ',' << speedup << ',' << speedup / workers << ','
                << (hardware_model ? std::to_string(topology.cores.size()) : "no_aplica") << ','
                << (hardware_model ? std::to_string(topology.logical_count()) : "no_aplica") << ','
                << (hardware_model ? topology.smt_active : "no_aplica") << ','
                << (hardware_model ? cpu_list(cpus) : "no_aplica") << '\n';
        }
        const auto result = summarize(samples, workers);
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
    raw.close(); summary.close();
    if (!raw || !summary) throw std::runtime_error("Error al escribir los CSV de métricas");
    std::cout << "mediciones_csv=" << raw_path.string() << " resumen_csv=" << summary_path.string() << '\n';
}
} // namespace rt
