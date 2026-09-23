#include "display.hpp"
#include "sequential.hpp"
#include "options.hpp"

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
                         "Por defecto: 640x480, tiles 32x32, 128 muestras, 12 rebotes.\n"
                         "La escena se muestra al terminar. Esc cierra la ventana.\n";
            return 0;
        }
        rt::Image image(options.width, options.height);
        rt::Scene scene = rt::default_scene();
        scene.settings.samples = options.samples;
        scene.settings.max_bounces = options.max_bounces;
        // Cámara baja y frontal, próxima al suelo y dirigida hacia las esferas.
        const rt::Camera camera(options.width, options.height, {0, 0.45, 2.0}, {0, 0.25, -7.5});
        const rt::TileGrid tiles(options.width, options.height, options.tile_width, options.tile_height);
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
        // FUTURA PARALELIZACIÓN: aquí se seleccionará el modelo de ejecución.
        // Antes de presentar o guardar el buffer, el modelo con hilos deberá
        // esperar la finalización de todos sus trabajadores (join o equivalente).
        // Los eventos y la presentación SDL seguirán en este hilo principal.
        const auto stats = rt::render_sequential(scene, camera, image, tiles, hooks);
        const auto completed = stats.completed;
        const auto render_time = stats.render_time;
        const auto loop_time = stats.loop_time;
        const bool complete = completed == tiles.size();
        // Secuencial significa sin paralelismo. La presentación es independiente:
        // mostrar toda la escena de una vez, fuera del tiempo del recorrido.
        if (complete && display) display->present(image);
        std::cout << std::fixed << std::setprecision(3)
                  << "estado=" << (complete ? "completo" : "cancelado")
                  << " modo=" << (options.headless ? "sin_ventana" : "ventana")
                  << " resolucion=" << options.width << 'x' << options.height
                  << " tile=" << options.tile_width << 'x' << options.tile_height
                  << " muestras=" << options.samples << " rebotes=" << options.max_bounces
                  << " tiles=" << completed << '/' << tiles.size() << '\n'
                  << "render_ms=" << milliseconds(render_time)
                  << " recorrido_ms=" << milliseconds(loop_time) << '\n';
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
