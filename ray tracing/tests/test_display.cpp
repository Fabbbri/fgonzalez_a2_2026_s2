/**
 * @file test_display.cpp
 * Integración de presentación y cancelación SDL con los modelos de tiles.
 * El controlador dummy permite inyectar eventos sin escritorio real.
 * Se comprueba que cerrar la ventana impide iniciar nuevos trabajadores.
 */
#include "app/display.hpp"
#include "execution/execution.hpp"
#include <SDL.h>
#include <iostream>
#include <stdexcept>

// Ensayo autónomo con recursos RAII; un fallo deja diagnóstico y salida no exitosa.
int main() {
    try {
        // Resolución impar y tiles no divisores ejercitan recortes y presentación.
        rt::Display display(33, 17);
        rt::Image image(33, 17);
        rt::Scene scene;
        scene.settings.samples = 1;
        const rt::Camera camera(33, 17);
        const rt::TileGrid tiles(33, 17, 8, 6);
        display.present(image);
        rt::RenderHooks hooks;
        // SDL se consulta desde el coordinador; los trabajadores solo leen cancelación.
        hooks.keep_running = [&] { return display.poll(); };
        const auto topology = rt::detect_cpu_topology();
        // Verifica render completo y presentación con los dos modelos de afinidad.
        for (const bool smt : {false, true}) {
            const auto full = smt ? rt::render_smt(scene, camera, image, tiles, topology, 1, hooks)
                                  : rt::render_cmp(scene, camera, image, tiles, topology, 1, hooks);
            if (full.completed != tiles.size()) throw std::runtime_error("SMT/CMP incompleto con SDL");
            display.present(image);
        }
        // Prueba coarse con esperas breves y notificación de completitud.
        const auto stats = rt::render_coarse(scene, camera, image, tiles, 1, 1, hooks);
        if (stats.completed != tiles.size()) throw std::runtime_error("Render incompleto");
        display.present(image);
        // Evento sintético de cierre, inicializado en cero antes de asignar su tipo.
        SDL_Event event{};
        event.type = SDL_QUIT;
        // La siguiente consulta de eventos debe cancelar antes de crear trabajadores.
        SDL_PushEvent(&event);
        const auto cancelled = rt::render_coarse(scene, camera, image, tiles, 1, 10000, hooks);
        if (cancelled.completed != 0 || cancelled.workers != 0) throw std::runtime_error("SDL no cancela");
        // Reinserta cierre para comprobar la misma propiedad en SMT y CMP.
        for (const bool smt : {false, true}) {
            SDL_PushEvent(&event);
            const auto stopped = smt ? rt::render_smt(scene, camera, image, tiles, topology, 1, hooks)
                                     : rt::render_cmp(scene, camera, image, tiles, topology, 1, hooks);
            if (stopped.completed || stopped.workers) throw std::runtime_error("SMT/CMP ignora cierre SDL");
        }
        std::cout << "OK: presentación y cierre SDL en el hilo principal\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
