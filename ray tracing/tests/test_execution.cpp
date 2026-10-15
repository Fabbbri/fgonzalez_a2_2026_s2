#include "execution/execution.hpp"
#include "app/options.hpp"
#include "metrics/metrics.hpp"
#include "metrics/benchmark.hpp"

#include <chrono>
#include <cstring>
#include <cmath>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#include <algorithm>
#ifdef __linux__
#include <sched.h>
#endif

namespace {
void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
bool equal(const rt::Image& a, const rt::Image& b) {
    return std::memcmp(a.data(), b.data(), static_cast<std::size_t>(a.width()) * a.height() * 3) == 0;
}
rt::Scene world() {
    rt::Scene scene;
    scene.settings.samples = 4;
    scene.settings.max_bounces = 8;
    scene.spheres = {{{0, 0, -4}, 1, {0.7, 0.2, 0.3}},
                     {{1.5, 0, -5}, 1, {1, 1, 1}, {rt::MaterialType::glass, 0, 1.5}},
                     {{0, -1001, -4}, 1000, {0.5, 0.5, 0.5}}};
    return scene;
}
void equivalence(int width, int height, int tw, int th) {
    const auto scene = world();
    const rt::Camera camera(width, height);
    const rt::TileGrid grid(width, height, tw, th);
    rt::Image reference(width, height);
    require(rt::render_sequential(scene, camera, reference, grid).completed == grid.size(), "Base incompleta");
    // Confirmar cobertura: incluso los bordes deben coincidir con el cálculo directo.
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        const auto pixel = rt::render_pixel(scene, camera, x, y);
        require(std::memcmp(&pixel, &reference.at(x, y), 3) == 0, "Cobertura incorrecta");
    }
    for (std::size_t k : {1u, 7u, 10000u}) {
        rt::Image fine(width, height);
        const auto stats = rt::render_fine(scene, camera, fine, grid, k);
        std::size_t turns = 0;
        for (std::size_t i = 0; i < grid.size(); ++i) turns += (rt::tile_pixels(grid.at(i)) + k - 1) / k;
        require(stats.quanta == turns && stats.completed == grid.size(), "Turnos de K incorrectos");
        require(equal(reference, fine), "Fine cambia la imagen");
    }
    rt::Image coarse(width, height);
    std::vector<int> notified(grid.size());
    const auto main_thread = std::this_thread::get_id();
    rt::RenderHooks hooks;
    hooks.keep_running = [&] { require(std::this_thread::get_id() == main_thread, "Eventos en trabajador"); return true; };
    hooks.tile_completed = [&](const rt::Tile& tile) {
        require(std::this_thread::get_id() == main_thread, "Notificación en trabajador");
        for (std::size_t i = 0; i < grid.size(); ++i) {
            const auto t = grid.at(i);
            if (t.x0 == tile.x0 && t.y0 == tile.y0) ++notified[i];
        }
        for (int y = tile.y0; y < tile.y1; ++y) for (int x = tile.x0; x < tile.x1; ++x)
            require(std::memcmp(&reference.at(x, y), &coarse.at(x, y), 3) == 0, "Tile publicado antes de terminar");
    };
    const auto stats = rt::render_coarse(scene, camera, coarse, grid, 0, 0, hooks);
    require(stats.completed == grid.size() && stats.workers == grid.size() && stats.blocks == 0, "Coarse incompleto");
    for (int n : notified) require(n == 1, "Notificación omitida o duplicada");
    require(equal(reference, coarse), "Coarse cambia la imagen");
    for (std::size_t p : {1u, 2u}) if (p <= grid.size()) {
        const auto limited = rt::render_coarse(scene, camera, coarse, grid, 0, 0, {}, p);
        require(limited.workers == p && limited.completed == grid.size() && equal(reference, coarse),
                "La variante con P trabajadores cambia la imagen o los tiles");
    }
}
void scheduling() {
    rt::Scene scene;
    scene.settings.samples = 1;
    const rt::Camera camera(5, 1);
    const rt::TileGrid grid(5, 1, 3, 1);
    rt::Image image(5, 1);
    std::vector<int> order;
    rt::RenderHooks hooks;
    hooks.tile_completed = [&](const rt::Tile& tile) { order.push_back(tile.x0); };
    rt::render_fine(scene, camera, image, grid, 1, hooks);
    require(order == std::vector<int>({3, 0}), "Fine no alterna tiles en round-robin");

    rt::TileProgress progress{grid.at(0)};
    int polls = 0;
    require(!rt::render_tile_step(scene, camera, image, progress, 3, [&] { return ++polls < 3; }), "Cancelación ignorada");
    require(progress.next == 2, "Progreso incluye un píxel sin terminar");
    require(rt::render_tile_step(scene, camera, image, progress, 1), "No reanuda el último píxel");
    bool rejected = false;
    try { rt::render_fine(scene, camera, image, grid, 0); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "K=0 aceptado");
}
void blocking_and_cancel() {
    using Clock = std::chrono::steady_clock;
    auto scene = world();
    scene.settings.samples = 64;
    const rt::Camera camera(96, 64);
    const rt::TileGrid grid(96, 64, 96, 64);
    rt::Image reference(96, 64), coarse(96, 64);
    rt::render_sequential(scene, camera, reference, grid);
    const auto full = rt::render_coarse(scene, camera, coarse, grid, 1, 1);
    require(full.blocks > 0 && equal(reference, coarse), "Bloqueos no conservan progreso");

    const auto start = Clock::now();
    rt::RenderHooks hooks;
    hooks.keep_running = [&] { return Clock::now() - start < std::chrono::milliseconds(30); };
    const auto cancelled = rt::render_coarse(scene, camera, coarse, grid, 1, 10000, hooks);
    require(cancelled.blocks > 0 && cancelled.completed == 0, "No cancela durante bloqueo");
    require(Clock::now() - start < std::chrono::seconds(2), "La espera bloquea la cancelación");

    hooks.keep_running = [] { return false; };
    require(rt::render_coarse(scene, camera, coarse, grid, 1, 1, hooks).workers == 0, "Arranca hilos después de cancelar");
    require(rt::render_fine(scene, camera, coarse, grid, 7, hooks).completed == 0, "Fine ignora cancelación");
    int polls = 0;
    hooks.keep_running = [&]() -> bool {
        if (++polls > 1) throw std::runtime_error("fallo de evento");
        return true;
    };
    bool propagated = false;
    try { rt::render_coarse(scene, camera, coarse, grid, 1, 10000, hooks); }
    catch (const std::runtime_error&) { propagated = true; }
    require(propagated, "No propaga excepción del callback tras detener trabajadores");
    rt::Image too_small(1, 1);
    propagated = false;
    try { rt::render_coarse(scene, camera, too_small, grid, 0, 0); }
    catch (const std::invalid_argument&) { propagated = true; }
    require(propagated, "No propaga errores del trabajador tras join");
}
void arguments() {
    const auto parse = [](std::vector<std::string> args) {
        args.insert(args.begin(), "raytracer");
        std::vector<char*> argv;
        for (auto& arg : args) argv.push_back(arg.data());
        return rt::parse_options(static_cast<int>(argv.size()), argv.data());
    };
    const auto fine = parse({"--model", "fine", "--quantum", "7"});
    require(fine.model == rt::ExecutionModel::fine && fine.quantum == 7, "Opciones fine incorrectas");
    const auto coarse = parse({"--model", "coarse", "--block-interval-ms", "0", "--block-duration-ms", "0"});
    require(coarse.model == rt::ExecutionModel::coarse && coarse.block_interval_ms == 0, "Opciones coarse incorrectas");
    const auto bench = parse({"--benchmark", "--model", "coarse", "--thread-counts", "1,2,4"});
    require(bench.headless && bench.repetitions == 200 && bench.thread_counts == std::vector<int>({1,2,4}), "Opciones de medición incorrectas");
    for (const auto& name : {"smt", "cmp"}) {
        const auto parallel = parse({"--benchmark", "--model", name, "--thread-counts", "1,2"});
        require(parallel.thread_counts == std::vector<int>({1,2}), "Barrido SMT/CMP rechazado");
    }
    require(parse({"--cpu-info"}).cpu_info, "No permite consultar topología CPU");
    require(parse({"--benchmark", "--model", "fine", "--baseline-csv", "base.csv"}).baseline_csv == "base.csv",
            "No permite reutilizar la referencia");
    for (const auto& args : std::vector<std::vector<std::string>>{{"--quantum", "0"}, {"--model", "inventado"}, {"--block-duration-ms", "-1"}, {"--model"},
             {"--benchmark", "--repetitions", "1"}, {"--model", "fine", "--threads", "2"},
             {"--benchmark", "--model", "coarse", "--thread-counts", "1,"},
             {"--model", "fine", "--baseline-csv", "base.csv"},
             {"--benchmark", "--baseline-csv", "base.csv"}}) {
        bool rejected = false;
        try { parse(args); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Argumentos inválidos aceptados");
    }
}
void parallel_models() {
    // Topología sintética para verificar políticas incluso si el equipo no tiene SMT.
    const rt::CpuTopology fake{{{2, 10}, {4, 12}, {6}}, "1"};
    require(rt::cmp_cpus(fake, 20) == std::vector<int>({2,4,6}), "CMP usa hermanos del mismo núcleo");
    require(rt::smt_cpus(fake, 20, 6) == std::vector<int>({2,4,6,10,12,2}), "SMT no reparte hermanos/sobresuscripción");
    const rt::CpuTopology no_smt{{{2}, {4}}, "0"};
    require(rt::smt_cpus(no_smt, 20) == std::vector<int>({2,4,2,4}), "No aproxima SMT sin hermanos disponibles");
    require(rt::cmp_cpus(fake, 1).size() == 1 && rt::smt_cpus(fake, 1).size() == 1, "Crea más trabajadores que tiles");
    bool rejected = false;
    try { rt::cmp_cpus(fake, 20, 4); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "CMP permite P mayor que C");

    const auto detected = rt::detect_cpu_topology();
    rt::CpuTopology topology = detected;
    topology.cores.resize(std::min<std::size_t>(2, topology.cores.size()));
#ifdef __linux__
    // Comprobar afinidad real sin modificar la del hilo principal ni de sequential.
    int observed = -1;
    std::exception_ptr pin_error;
    const int wanted = topology.cores.front().front();
    std::thread probe([&] {
        try { rt::pin_current_thread(wanted); observed = sched_getcpu(); }
        catch (...) { pin_error = std::current_exception(); }
    });
    probe.join();
    if (pin_error) std::rethrow_exception(pin_error);
    require(observed == wanted, "Trabajador no ejecuta en su CPU asignada");
    require(rt::detect_cpu_topology().cores == detected.cores, "La afinidad altera el principal/sequential");
#endif
    const auto scene = world();
    const rt::Camera camera(41, 29);
    const rt::TileGrid grid(41, 29, 9, 8);
    rt::Image reference(41, 29), image(41, 29);
    rt::render_sequential(scene, camera, reference, grid);
    const auto main_thread = std::this_thread::get_id();
    for (const bool smt : {false, true}) {
        const auto run = [&](const rt::RenderHooks& hooks, const rt::CpuTopology& cpu) {
            return smt ? rt::render_smt(scene, camera, image, grid, cpu, 0, hooks)
                       : rt::render_cmp(scene, camera, image, grid, cpu, 0, hooks);
        };
        std::size_t notifications = 0;
        rt::RenderHooks hooks;
        hooks.keep_running = [&] {
            require(std::this_thread::get_id() == main_thread, "SMT/CMP invoca eventos en trabajador");
            return true;
        };
        hooks.tile_completed = [&](const rt::Tile& tile) {
            require(std::this_thread::get_id() == main_thread, "SMT/CMP notifica en trabajador");
            ++notifications;
            for (int y = tile.y0; y < tile.y1; ++y) for (int x = tile.x0; x < tile.x1; ++x)
                require(std::memcmp(&reference.at(x,y), &image.at(x,y), 3) == 0, "Publica tile parcial");
        };
        const auto full = run(hooks, topology);
        const auto expected = smt ? rt::smt_cpus(topology, grid.size()).size() : topology.cores.size();
        require(full.workers == expected && full.completed == grid.size() && full.blocks == 0 &&
                notifications == grid.size() && equal(image, reference), "SMT/CMP cambia imagen o cobertura");
        hooks.keep_running = [] { return false; };
        require(run(hooks, topology).workers == 0, "SMT/CMP crea hilos tras cancelar");
        hooks.tile_completed = {};
        int polls = 0;
        hooks.keep_running = [&]() -> bool {
            if (++polls > 1) throw std::runtime_error("fallo evento");
            return true;
        };
        rejected = false;
        try { run(hooks, topology); } catch (const std::runtime_error&) { rejected = true; }
        require(rejected, "SMT/CMP no propaga callback tras join");
        const rt::CpuTopology bad{{{-1}}, "?"};
        rejected = false;
        try { run({}, bad); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "SMT/CMP ignora error de afinidad del trabajador");

        auto heavy = world();
        heavy.settings.samples = 256;
        const rt::Camera heavy_camera(96, 64);
        const rt::TileGrid heavy_tiles(96, 64, 32, 32);
        rt::Image heavy_image(96, 64);
        const auto start = std::chrono::steady_clock::now();
        rt::RenderHooks cancel;
        cancel.keep_running = [&] {
            return std::chrono::steady_clock::now() - start < std::chrono::milliseconds(10);
        };
        const auto stopped = smt ? rt::render_smt(heavy, heavy_camera, heavy_image, heavy_tiles, topology, 1, cancel)
                                 : rt::render_cmp(heavy, heavy_camera, heavy_image, heavy_tiles, topology, 1, cancel);
        require(stopped.completed < heavy_tiles.size() && stopped.workers == 1, "No cancela SMT/CMP en marcha");
        require(std::chrono::steady_clock::now() - start < std::chrono::seconds(2), "SMT/CMP no responde al cancelar");
    }
}
void metrics() {
    const auto result = rt::summarize({{100, 50}, {100, 25}}, 2);
    require(result.total.mean == 37.5 && std::abs(result.total.deviation - std::sqrt(312.5)) < 1e-10, "Estadística de tiempos incorrecta");
    require(result.speedup.mean == 3 && result.efficiency.mean == 1.5, "Speedup o eficiencia incorrectos");
    const auto shared = rt::summarize_shared({{100,50},{100,25}}, {80,120}, 2);
    const double ratio = 100.0 / 37.5;
    require(std::abs(shared.speedup.mean - ratio) < 1e-10 &&
            std::abs(shared.efficiency.mean - ratio/2) < 1e-10,
            "Referencia compartida debe usar cociente de medias, no media de cocientes");
    const double expected_margin = 1.96 * ratio * std::sqrt(
        (800.0 / 10000.0) / 2 + (312.5 / (37.5*37.5)) / 2);
    require(std::abs(shared.speedup.ci_high - ratio - expected_margin) < 1e-10,
            "IC compartido debe considerar varianza de ambas medias");
    const double margin = 1.96 * result.total.deviation / std::sqrt(2.0);
    require(std::abs(result.total.ci_low - (37.5 - margin)) < 1e-10, "IC95 de la media incorrecto");
    bool rejected = false;
    try { rt::summarize({{1,0},{1,1}}, 1); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "Permite división por un tiempo cero");

    rt::Options options;
    options.benchmark = true;
    options.headless = true;
    options.width = 11; options.height = 7;
    options.repetitions = 2;
    options.thread_counts = {1,2,4};
    options.model = rt::ExecutionModel::coarse;
    options.block_interval_ms = 0;
    options.metrics_output = "build/test-metrics.csv";
    auto scene = world();
    const rt::Camera camera(11,7);
    const rt::TileGrid tiles(11,7,4,3);
    rt::benchmark(options, scene, camera, tiles);
    std::ifstream raw("build/test-metrics.csv");
    std::string line;
    std::getline(raw, line);
    int rows = 0;
    while (std::getline(raw, line)) {
        std::stringstream record(line);
        std::vector<std::string> cells;
        std::string cell;
        while (std::getline(record,cell,',')) cells.push_back(cell);
        require(cells.size() == 25, "CSV incompleto");
        const double p = std::stod(cells[1]), total = std::stod(cells[13]), seq = std::stod(cells[14]);
        const double speedup = std::stod(cells[15]), efficiency = std::stod(cells[16]);
        require(std::abs(speedup - seq/total) < 1e-8 && std::abs(efficiency - speedup/p) < 1e-8, "Fórmulas exportadas incorrectas");
        ++rows;
    }
    require(rows == 6, "Faltan configuraciones de escalabilidad o repeticiones");

    // La referencia se mide una vez y permite un n distinto para cada modelo.
    options.thread_counts.clear();
    options.model = rt::ExecutionModel::sequential;
    options.repetitions = 3;
    options.metrics_output = "build/test-baseline.csv";
    rt::benchmark(options, scene, camera, tiles);
    options.model = rt::ExecutionModel::fine;
    options.repetitions = 2;
    options.baseline_csv = "build/test-baseline.csv";
    options.metrics_output = "build/test-shared.csv";
    rt::benchmark(options, scene, camera, tiles);
    std::ifstream imported("build/test-shared.csv");
    std::getline(imported, line);
    rows = 0;
    double reference_mean = 0;
    while (std::getline(imported, line)) {
        std::stringstream record(line);
        std::vector<std::string> cells;
        std::string cell;
        while (std::getline(record,cell,',')) cells.push_back(cell);
        require(cells[21] == "compartida", "Falta identificar la referencia compartida");
        if (rows) require(std::stod(cells[14]) == reference_mean, "Referencia varía entre filas");
        reference_mean = std::stod(cells[14]);
        ++rows;
    }
    require(rows == 2, "Modelo con referencia compartida exporta repeticiones incorrectas");
    options.samples++;
    rejected = false;
    try { rt::benchmark(options, scene, camera, tiles); } catch (const std::invalid_argument&) { rejected = true; }
    require(rejected, "Acepta referencia con carga diferente");
}
} // namespace
int main() {
    try {
        equivalence(41, 29, 9, 8);
        equivalence(5, 3, 100, 100);
        equivalence(1, 1, 1, 1);
        scheduling();
        blocking_and_cancel();
        arguments();
        parallel_models();
        metrics();
        std::cout << "OK: igualdad de modelos, bordes, K, round-robin, bloqueos, callbacks y cancelación\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
