#include "native_view.hpp"
#include <SDL.h>
#include <iostream>
#include <stdexcept>

using namespace nbody;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void key(SDL_Keycode code) {
    SDL_Event event{};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = code;
    check(SDL_PushEvent(&event) == 1, "No se pudo inyectar el evento");
}

void compare(const std::vector<Body>& actual, const std::vector<Body>& expected) {
    check(actual.size() == expected.size(), "Cantidad distinta de cuerpos");
    for (std::size_t i = 0; i < actual.size(); ++i) {
        check(actual[i].position.x == expected[i].position.x && actual[i].position.y == expected[i].position.y &&
              actual[i].velocity.x == expected[i].velocity.x && actual[i].velocity.y == expected[i].velocity.y &&
              actual[i].acceleration.x == expected[i].acceleration.x && actual[i].acceleration.y == expected[i].acceleration.y,
              "La visualización altera el resultado físico");
    }
}

int main() try {
    check(SDL_Init(SDL_INIT_VIDEO) == 0, "No se pudo iniciar el controlador de prueba");
    Config config;
    config.bodies_per_galaxy = 2;
    config.steps = 10;
    auto expected = create_galaxies(config.bodies_per_galaxy, config.seed);
    std::vector<Vec2> scratch;
    refresh_accelerations(expected, scratch);
    for (std::size_t i = 0; i < config.steps; ++i) advance_one_step(expected, scratch, config.dt);
    auto bodies = create_galaxies(config.bodies_per_galaxy, config.seed);
    key(SDLK_PLUS);  // La velocidad visual no cambia dt ni los resultados.
    auto result = run_native(bodies, scratch, config, true);
    check(result.completed_steps == config.steps, "No completa los pasos visuales");
    compare(bodies, expected);

    config.steps = 1;
    bodies = create_galaxies(config.bodies_per_galaxy, config.seed);
    expected = bodies;
    refresh_accelerations(expected, scratch);
    advance_one_step(expected, scratch, config.dt);
    key(SDLK_SPACE);
    key(SDLK_n);
    key(SDLK_f);
    result = run_native(bodies, scratch, config, true);
    check(result.completed_steps == 1, "El avance manual no funciona");
    compare(bodies, expected);

    // R recupera la semilla y condiciones iniciales, aunque el estado cambió.
    key(SDLK_r);
    key(SDLK_n);
    result = run_native(bodies, scratch, config, true);
    check(result.completed_steps == 1, "El reinicio no restablece el contador");
    compare(bodies, expected);

    bodies = create_galaxies(config.bodies_per_galaxy, config.seed);
    expected = bodies;
    refresh_accelerations(expected, scratch);
    key(SDLK_ESCAPE);
    result = run_native(bodies, scratch, config, true);
    check(result.completed_steps == 0, "Esc no detiene la ejecución");
    compare(bodies, expected);
    SDL_Quit();
    std::cout << "Pruebas de ventana, controles y equivalencia física correctas.\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    SDL_Quit();
    return 1;
}
