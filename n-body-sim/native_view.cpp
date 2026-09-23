#include "native_view.hpp"
#include "bitmap_text.hpp"
#include <SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace nbody {
namespace {
using Clock = std::chrono::steady_clock;
constexpr SDL_Color cyan{103, 232, 249, 255};
constexpr SDL_Color orange{255, 188, 112, 255};
constexpr SDL_Color white{229, 237, 248, 255};
constexpr SDL_Color muted{145, 164, 190, 255};
constexpr int top = 90;
constexpr int bottom = 90;

struct VideoSession {
    VideoSession() {
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
            throw std::runtime_error(std::string("No se pudo iniciar SDL2: ") + SDL_GetError());
        }
    }
    ~VideoSession() { SDL_QuitSubSystem(SDL_INIT_VIDEO); }
};

// Los recursos gráficos se liberan también si la simulación lanza una excepción.
struct View {
    VideoSession session;
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window{nullptr, SDL_DestroyWindow};
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer{nullptr, SDL_DestroyRenderer};
    double center_x = 0;
    double center_y = 0;
    double half_height = 4;

    View() {
        window.reset(SDL_CreateWindow("Dos galaxias - simulacion nativa 2D",
                     SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1100, 720,
                     SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE));
        if (!window) throw std::runtime_error(SDL_GetError());
        SDL_SetWindowMinimumSize(window.get(), 850, 480);
        renderer.reset(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED));
        if (!renderer) renderer.reset(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_SOFTWARE));
        if (!renderer) throw std::runtime_error(SDL_GetError());
    }

    double scale(int height) const { return (height - top - bottom) / (2.0 * half_height); }

    void fit(const std::vector<Body>& bodies) {
        double min_x = bodies.front().position.x, max_x = min_x;
        double min_y = bodies.front().position.y, max_y = min_y;
        for (const Body& body : bodies) {
            min_x = std::min(min_x, body.position.x); max_x = std::max(max_x, body.position.x);
            min_y = std::min(min_y, body.position.y); max_y = std::max(max_y, body.position.y);
        }
        int width, height;
        SDL_GetRendererOutputSize(renderer.get(), &width, &height);
        center_x = min_x / 2 + max_x / 2;
        center_y = min_y / 2 + max_y / 2;
        half_height = std::clamp(1.25 * std::max({1.0, (max_y - min_y) / 2,
            (max_x - min_x) / 2 * (height - top - bottom) / width}), 0.02, 1e6);
    }

    void camera_event(const SDL_Event& event) {
        if (event.type == SDL_MOUSEWHEEL) {
            const int direction = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1;
            half_height = std::clamp(half_height * std::pow(0.85, direction * event.wheel.y), 0.02, 1e6);
        } else if (event.type == SDL_MOUSEMOTION && (event.motion.state & SDL_BUTTON_LMASK)) {
            int width, height;
            SDL_GetRendererOutputSize(renderer.get(), &width, &height);
            center_x -= event.motion.xrel / scale(height);
            center_y += event.motion.yrel / scale(height);
        }
    }

    void draw(const std::vector<Body>& bodies, const Config& config,
              std::size_t step, bool paused, double speed) {
        auto* render = renderer.get();
        int width, height;
        SDL_GetRendererOutputSize(render, &width, &height);
        if (height <= top + bottom || width <= 0) return;
        SDL_SetRenderDrawColor(render, 8, 14, 25, 255);
        SDL_RenderClear(render);
        const double pixels = scale(height);
        const double middle_y = top + (height - top - bottom) / 2.0;
        const auto px = [&](double x) { return width / 2.0 + (x - center_x) * pixels; };
        const auto py = [&](double y) { return middle_y - (y - center_y) * pixels; };
        const SDL_Rect viewport{0, top, width, height - top - bottom};
        SDL_RenderSetClipRect(render, &viewport);

        // Escala idéntica en ambos ejes. Las marcas quedan a unos 80 píxeles.
        const double desired = 80 / pixels;
        const double magnitude = std::pow(10.0, std::floor(std::log10(desired)));
        double grid = magnitude;
        for (double factor : {1.0, 2.0, 5.0, 10.0}) {
            grid = factor * magnitude;
            if (grid >= desired) break;
        }
        const double left = center_x - width / (2 * pixels);
        const double low = center_y - half_height;
        for (int tick = 0; tick <= static_cast<int>(std::ceil(width / pixels / grid)); ++tick) {
            const double x = (std::ceil(left / grid) + tick) * grid;
            const int screen = static_cast<int>(px(x));
            SDL_SetRenderDrawColor(render, 30, 43, 64, 255);
            SDL_RenderDrawLine(render, screen, top, screen, height - bottom);
        }
        for (int tick = 0; tick <= static_cast<int>(std::ceil(2 * half_height / grid)); ++tick) {
            const double y = (std::ceil(low / grid) + tick) * grid;
            const int screen = static_cast<int>(py(y));
            SDL_SetRenderDrawColor(render, 30, 43, 64, 255);
            SDL_RenderDrawLine(render, 0, screen, width, screen);
        }

        std::size_t visible = 0;
        for (const Body& body : bodies) {
            const double x = px(body.position.x), y = py(body.position.y);
            if (!(x >= 0 && x < width && y >= top && y < height - bottom)) continue;
            ++visible;
            const SDL_Color color = body.galaxy_id == 0 ? cyan : orange;
            SDL_SetRenderDrawColor(render, color.r, color.g, color.b, color.a);
            const int sx = static_cast<int>(x), sy = static_cast<int>(y);
            // Una pequeña cruz de píxeles mantiene visibles los cuerpos aislados.
            const SDL_Rect horizontal{sx - 2, sy - 1, 5, 3};
            const SDL_Rect vertical{sx - 1, sy - 2, 3, 5};
            SDL_RenderFillRect(render, &horizontal);
            SDL_RenderFillRect(render, &vertical);
        }
        SDL_RenderSetClipRect(render, nullptr);

        draw_text(render, "DOS GALAXIAS / 2D", 20, 16, white);
        draw_text(render, "GALAXIA 0", 20, 47, cyan);
        draw_text(render, "GALAXIA 1", 165, 47, orange);
        const char* state = step == config.steps ? "COMPLETA" : paused ? "PAUSA" : "EN MARCHA";
        draw_text(render, state, width - 175, 16, white);
        std::ostringstream status;
        status << std::fixed << std::setprecision(3) << "T " << step * config.dt
               << "  PASO " << step << '/' << config.steps;
        draw_text(render, status.str(), 320, 47, white);
        std::ostringstream camera;
        camera << "CUERPOS " << bodies.size() << "  FUERA DE VISTA " << bodies.size() - visible
               << "  VELOCIDAD " << speed << " U/S  REJILLA " << std::setprecision(3) << grid;
        draw_text(render, camera.str(), 20, height - 77, muted);
        draw_text(render, "ESPACIO PAUSA   N PASO   R REINICIAR   F ENCUADRAR", 20, height - 50, muted);
        draw_text(render, "+/- VELOCIDAD   RUEDA ZOOM   ARRASTRAR CAMARA   ESC SALIR", 20, height - 23, muted);
        SDL_RenderPresent(render);
    }
};
}  // namespace

RunResult run_native(std::vector<Body>& bodies, std::vector<Vec2>& accelerations,
                     const Config& config, bool close_when_complete) {
    View view;
    view.fit(bodies);
    RunResult result;
    auto start = Clock::now();
    refresh_accelerations(bodies, accelerations);
    result.compute_seconds = std::chrono::duration<double>(Clock::now() - start).count();
    bool paused = false;
    bool quit = false;
    double speed = 1.0;
    double accumulated = 0.0;
    auto previous = Clock::now();

    while (!quit) {
        const auto frame_start = Clock::now();
        const double elapsed = std::chrono::duration<double>(frame_start - previous).count();
        previous = frame_start;
        bool single_step = false;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT ||
                (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)) quit = true;
            view.camera_event(event);
            if (event.type != SDL_KEYDOWN || event.key.repeat) continue;
            switch (event.key.keysym.sym) {
            case SDLK_ESCAPE: quit = true; break;
            case SDLK_SPACE: paused = !paused; accumulated = 0; break;
            case SDLK_n: paused = true; single_step = true; accumulated = 0; break;
            case SDLK_f: view.fit(bodies); break;
            case SDLK_PLUS: case SDLK_EQUALS: case SDLK_KP_PLUS: speed = std::min(speed * 2, 16.0); break;
            case SDLK_MINUS: case SDLK_KP_MINUS: speed = std::max(speed / 2, 0.125); break;
            case SDLK_r:
                bodies = create_galaxies(config.bodies_per_galaxy, config.seed);
                result = {};
                start = Clock::now();
                refresh_accelerations(bodies, accelerations);
                result.compute_seconds = std::chrono::duration<double>(Clock::now() - start).count();
                accumulated = 0;
                paused = true;
                single_step = false;
                view.fit(bodies);
                break;
            default: break;
            }
        }
        if (quit) break;
        if (!paused) accumulated = std::min(accumulated + std::min(elapsed, 0.1) * speed, std::max(0.25 * speed, config.dt));

        // Siempre usamos el mismo dt. La velocidad visual solo cambia cuántos
        // pasos se ejecutan antes de dibujar; nunca altera el integrador.
        const auto batch_start = Clock::now();
        while (result.completed_steps < config.steps && (single_step || (!paused && accumulated >= config.dt))) {
            advance_one_step(bodies, accelerations, config.dt);
            ++result.completed_steps;
            if (single_step) { single_step = false; break; }
            accumulated -= config.dt;
            // Conserva respuesta a eventos cuando la simulación es costosa.
            if (Clock::now() - batch_start > std::chrono::milliseconds(8)) break;
        }
        result.compute_seconds += std::chrono::duration<double>(Clock::now() - batch_start).count();
        if (result.completed_steps == config.steps) paused = true;
        view.draw(bodies, config, result.completed_steps, paused, speed);
        if (close_when_complete && result.completed_steps == config.steps) break;

        const double frame_ms = std::chrono::duration<double, std::milli>(Clock::now() - frame_start).count();
        if (frame_ms < 16.0) SDL_Delay(static_cast<Uint32>(16.0 - frame_ms));
    }
    return result;
}
}  // namespace nbody
