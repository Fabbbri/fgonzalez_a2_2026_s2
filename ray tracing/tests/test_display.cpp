#include "app/display.hpp"
#include "execution/execution.hpp"
#include <SDL.h>
#include <iostream>
#include <stdexcept>

int main() {
    try {
        rt::Display display(33, 17);
        rt::Image image(33, 17);
        rt::Scene scene;
        scene.settings.samples = 1;
        const rt::Camera camera(33, 17);
        const rt::TileGrid tiles(33, 17, 8, 6);
        display.present(image);
        rt::RenderHooks hooks;
        hooks.keep_running = [&] { return display.poll(); };
        const auto topology = rt::detect_cpu_topology();
        for (const bool smt : {false, true}) {
            const auto full = smt ? rt::render_smt(scene, camera, image, tiles, topology, 1, hooks)
                                  : rt::render_cmp(scene, camera, image, tiles, topology, 1, hooks);
            if (full.completed != tiles.size()) throw std::runtime_error("SMT/CMP incompleto con SDL");
            display.present(image);
        }
        const auto stats = rt::render_coarse(scene, camera, image, tiles, 1, 1, hooks);
        if (stats.completed != tiles.size()) throw std::runtime_error("Render incompleto");
        display.present(image);
        SDL_Event event{};
        event.type = SDL_QUIT;
        SDL_PushEvent(&event);
        const auto cancelled = rt::render_coarse(scene, camera, image, tiles, 1, 10000, hooks);
        if (cancelled.completed != 0 || cancelled.workers != 0) throw std::runtime_error("SDL no cancela");
        for (const bool smt : {false, true}) {
            SDL_PushEvent(&event);
            const auto stopped = smt ? rt::render_smt(scene, camera, image, tiles, topology, 1, hooks)
                                     : rt::render_cmp(scene, camera, image, tiles, topology, 1, hooks);
            if (stopped.completed || stopped.workers) throw std::runtime_error("SMT/CMP ignora cierre SDL");
        }
        std::cout << "OK: presentación y cierre SDL en el hilo principal\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
