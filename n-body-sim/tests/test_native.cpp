#include "nbody/native_view.hpp"
#include <SDL.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace nbody;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void key(SDL_Keycode code) {
    SDL_Event event{}; event.type = SDL_KEYDOWN; event.key.keysym.sym = code;
    check(SDL_PushEvent(&event) == 1, "No se pudo inyectar el evento");
}
void compare(const std::vector<Body>& actual, const std::vector<Body>& expected) {
    check(actual.size() == expected.size(), "Cantidad distinta de cuerpos");
    for (std::size_t i = 0; i < actual.size(); ++i) {
        check(actual[i].mass == expected[i].mass && actual[i].galaxy_id == expected[i].galaxy_id &&
              actual[i].position.x == expected[i].position.x && actual[i].position.y == expected[i].position.y &&
              actual[i].velocity.x == expected[i].velocity.x && actual[i].velocity.y == expected[i].velocity.y &&
              actual[i].acceleration.x == expected[i].acceleration.x && actual[i].acceleration.y == expected[i].acceleration.y,
              "El archivo altera el estado físico");
    }
}
int main() try {
    const std::string path = "build/test-trajectory.nbody";
    Config config; config.bodies_per_galaxy = 2; config.steps = 3;
    auto bodies = create_galaxies(config.bodies_per_galaxy, config.seed);
    std::vector<Vec2> scratch;
    refresh_accelerations(bodies, scratch);
    std::vector<std::vector<Body>> expected{bodies};
    TrajectoryWriter writer(path, config, bodies.size()); writer.append(bodies);
    for (std::size_t step = 0; step < config.steps; ++step) {
        advance_one_step(bodies, scratch, config.dt); writer.append(bodies); expected.push_back(bodies);
    }
    writer.finish();
    TrajectoryReader trajectory(path);
    check(trajectory.frame_count() == 4 && trajectory.config().seed == config.seed &&
          trajectory.config().dt == config.dt, "Metadatos incorrectos");
    for (std::size_t i : {3, 0, 2, 1}) compare(trajectory.read_frame(i), expected[i]);
    bool rejected = false;
    try { trajectory.read_frame(4); } catch (const std::out_of_range&) { rejected = true; }
    check(rejected, "Admite frames fuera del archivo");
    check(SDL_Init(SDL_INIT_VIDEO) == 0, "No se pudo iniciar SDL dummy");
    key(SDLK_PLUS);
    check(replay_native(trajectory, true) == config.steps, "No completa la reproducción");
    key(SDLK_SPACE); key(SDLK_n); key(SDLK_f); key(SDLK_ESCAPE);
    check(replay_native(trajectory, true) == 0, "Esc no detiene la reproducción");
    Config single = config; single.steps = 1;
    TrajectoryWriter short_writer("build/test-single.nbody", single, expected[0].size());
    short_writer.append(expected[0]); short_writer.append(expected[1]); short_writer.finish();
    TrajectoryReader short_trajectory("build/test-single.nbody");
    key(SDLK_r); key(SDLK_n);
    check(replay_native(short_trajectory, true) == 1, "Reinicio/avance manual incorrecto");
    compare(short_trajectory.read_frame(1), expected[1]);
    Config zero = config; zero.steps = 0;
    TrajectoryWriter zero_writer("build/test-zero.nbody", zero, expected[0].size());
    zero_writer.append(expected[0]); zero_writer.finish();
    TrajectoryReader zero_trajectory("build/test-zero.nbody");
    check(replay_native(zero_trajectory, true) == 0, "No reproduce el estado inicial sin pasos");
    compare(zero_trajectory.read_frame(0), expected[0]);
    SDL_Quit();
    // finish() impide publicar como completo un archivo con frames faltantes.
    {
        TrajectoryWriter unfinished("build/test-broken.nbody", config, expected[0].size());
        unfinished.append(expected[0]);
        rejected = false;
        try { unfinished.finish(); } catch (const std::runtime_error&) { rejected = true; }
        check(rejected, "Finaliza una trayectoria incompleta");
    }
    std::filesystem::copy_file(path, "build/test-broken.nbody", std::filesystem::copy_options::overwrite_existing);
    std::filesystem::resize_file("build/test-broken.nbody", std::filesystem::file_size(path) - 1);
    rejected = false;
    try { TrajectoryReader broken("build/test-broken.nbody"); } catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "Acepta un archivo truncado");
    { std::ofstream invalid("build/test-broken.nbody", std::ios::binary | std::ios::trunc); invalid << "invalid"; }
    rejected = false;
    try { TrajectoryReader broken("build/test-broken.nbody"); } catch (const std::runtime_error&) { rejected = true; }
    check(rejected, "Acepta una cabecera inválida");
    for (const auto& file : {path, std::string("build/test-single.nbody"),
                            std::string("build/test-zero.nbody"), std::string("build/test-broken.nbody")}) {
        std::filesystem::remove(file);
    }
    std::cout << "Pruebas de trayectoria, reproducción y controles correctas.\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; SDL_Quit(); return 1; }
