#include "nbody.hpp"
#include <array>
#include <cctype>
#include <string_view>
#include <SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <deque>
#include <random>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace nbody {
namespace {
// Fuente mínima 5x7: cada fila usa cinco bits. No requiere fuentes externas.
std::array<unsigned char, 7> glyph(char character) {
    switch (character) {
    case 'A': return {14,17,17,31,17,17,17};
    case 'B': return {30,17,17,30,17,17,30};
    case 'C': return {14,17,16,16,16,17,14};
    case 'D': return {30,17,17,17,17,17,30};
    case 'E': return {31,16,16,30,16,16,31};
    case 'F': return {31,16,16,30,16,16,16};
    case 'G': return {14,17,16,23,17,17,15};
    case 'H': return {17,17,17,31,17,17,17};
    case 'I': return {14,4,4,4,4,4,14};
    case 'J': return {7,2,2,2,18,18,12};
    case 'K': return {17,18,20,24,20,18,17};
    case 'L': return {16,16,16,16,16,16,31};
    case 'M': return {17,27,21,21,17,17,17};
    case 'N': return {17,25,21,19,17,17,17};
    case 'O': return {14,17,17,17,17,17,14};
    case 'P': return {30,17,17,30,16,16,16};
    case 'Q': return {14,17,17,17,21,18,13};
    case 'R': return {30,17,17,30,20,18,17};
    case 'S': return {15,16,16,14,1,1,30};
    case 'T': return {31,4,4,4,4,4,4};
    case 'U': return {17,17,17,17,17,17,14};
    case 'V': return {17,17,17,17,17,10,4};
    case 'W': return {17,17,17,21,21,21,10};
    case 'X': return {17,17,10,4,10,17,17};
    case 'Y': return {17,17,10,4,4,4,4};
    case 'Z': return {31,1,2,4,8,16,31};
    case '0': return {14,17,19,21,25,17,14};
    case '1': return {4,12,4,4,4,4,14};
    case '2': return {14,17,1,2,4,8,31};
    case '3': return {30,1,1,14,1,1,30};
    case '4': return {2,6,10,18,31,2,2};
    case '5': return {31,16,16,30,1,1,30};
    case '6': return {14,16,16,30,17,17,14};
    case '7': return {31,1,2,4,8,8,8};
    case '8': return {14,17,17,14,17,17,14};
    case '9': return {14,17,17,15,1,1,14};
    case '.': return {0,0,0,0,0,12,12};
    case ':': return {0,12,12,0,12,12,0};
    case '-': return {0,0,0,31,0,0,0};
    case '+': return {0,4,4,31,4,4,0};
    case '/': return {1,2,2,4,8,8,16};
    default: return {};
    }
}

void draw_text(SDL_Renderer* renderer, std::string_view text, int x, int y,
                      SDL_Color color, int scale = 2) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    for (unsigned char character : text) {
        const auto rows = glyph(static_cast<char>(std::toupper(character)));
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if ((rows[row] & (1 << (4 - column))) != 0) {
                    const SDL_Rect pixel{x + column * scale, y + row * scale, scale, scale};
                    SDL_RenderFillRect(renderer, &pixel);
                }
            }
        }
        x += 6 * scale;
    }
}

using Clock = std::chrono::steady_clock;
constexpr SDL_Color cyan{103, 232, 249, 255};
constexpr SDL_Color orange{255, 188, 112, 255};
constexpr SDL_Color white{229, 237, 248, 255};
constexpr SDL_Color muted{145, 164, 190, 255};
constexpr int top = 112;
constexpr int bottom = 110;

struct VideoSession {
    VideoSession() {
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
            throw std::runtime_error(std::string("No se pudo iniciar SDL2: ") + SDL_GetError());
        }
    }
    ~VideoSession() { SDL_QuitSubSystem(SDL_INIT_VIDEO); }
};

// Historial acotado y muestreado en tiempo simulado: las estelas no dependen
// del FPS ni de la velocidad de reproducción. Solo almacena posiciones reales.
struct Trails {
    std::deque<std::vector<Vec2>> frames;
    std::size_t stride;
    explicit Trails(double dt) : stride(static_cast<std::size_t>(std::clamp(0.025 / dt, 1.0, 1e6))) {}
    void record(const std::vector<Body>& bodies, std::size_t step, bool force = false) {
        if (!force && step % stride != 0) return;
        std::vector<Vec2> points;
        points.reserve(bodies.size());
        for (const auto& body : bodies) points.push_back(body.position);
        frames.push_back(std::move(points));
        if (frames.size() > 32) frames.pop_front();
    }
};

// Los recursos gráficos se liberan también si la simulación lanza una excepción.
struct View {
    VideoSession session;
    std::unique_ptr<SDL_Window, decltype(&SDL_DestroyWindow)> window{nullptr, SDL_DestroyWindow};
    std::unique_ptr<SDL_Renderer, decltype(&SDL_DestroyRenderer)> renderer{nullptr, SDL_DestroyRenderer};
    double center_x = 0;
    double center_y = 0;
    double half_height = 4;
    bool show_grid = false;
    bool show_trails = true;
    std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> glow{nullptr, SDL_DestroyTexture};
    struct Star { double x, y; int brightness; };
    std::vector<Star> background;

    SDL_Rect timeline(int width, int height) const { return {28, height - 91, width - 56, 14}; }

    // Sprite radial generado una sola vez. La suma de halos hace visible la
    // densidad de partículas sin inventar brazos espirales ni cuerpos extra.
    void make_glow() {
        constexpr int size = 64;
        std::unique_ptr<SDL_Surface, decltype(&SDL_FreeSurface)> surface(
            SDL_CreateRGBSurfaceWithFormat(0, size, size, 32, SDL_PIXELFORMAT_RGBA32), SDL_FreeSurface);
        if (!surface) throw std::runtime_error(SDL_GetError());
        if (SDL_LockSurface(surface.get()) != 0) throw std::runtime_error(SDL_GetError());
        for (int y = 0; y < size; ++y) {
            auto* row = reinterpret_cast<Uint32*>(static_cast<Uint8*>(surface->pixels) + y * surface->pitch);
            for (int x = 0; x < size; ++x) {
                const double dx = (x - 31.5) / 31.5, dy = (y - 31.5) / 31.5;
                const double r2 = dx * dx + dy * dy;
                const auto alpha = static_cast<Uint8>(255 * std::max(0.0, std::exp(-4.5 * r2) - std::exp(-4.5)));
                row[x] = SDL_MapRGBA(surface->format, 255, 255, 255, alpha);
            }
        }
        SDL_UnlockSurface(surface.get());
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
        glow.reset(SDL_CreateTextureFromSurface(renderer.get(), surface.get()));
        if (!glow) throw std::runtime_error(SDL_GetError());
        SDL_SetTextureBlendMode(glow.get(), SDL_BLENDMODE_ADD);
    }

    void halo(int x, int y, int radius, SDL_Color color, Uint8 alpha) {
        SDL_SetTextureColorMod(glow.get(), color.r, color.g, color.b);
        SDL_SetTextureAlphaMod(glow.get(), alpha);
        const SDL_Rect target{x - radius, y - radius, 2 * radius, 2 * radius};
        SDL_RenderCopy(renderer.get(), glow.get(), nullptr, &target);
    }

    View() {
        window.reset(SDL_CreateWindow("Simulación de galaxias",
                     SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1100, 720,
                     SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI));
        if (!window) throw std::runtime_error(SDL_GetError());
        SDL_SetWindowMinimumSize(window.get(), 850, 480);
        renderer.reset(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_ACCELERATED));
        if (!renderer) renderer.reset(SDL_CreateRenderer(window.get(), -1, SDL_RENDERER_SOFTWARE));
        if (!renderer) throw std::runtime_error(SDL_GetError());
        make_glow();
        std::mt19937 random(2026);
        std::uniform_real_distribution<double> unit(0, 1);
        for (int i = 0; i < 180; ++i) background.push_back({unit(random), unit(random), 22 + static_cast<int>(unit(random) * 65)});
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
            int window_width, window_height;
            SDL_GetWindowSize(window.get(), &window_width, &window_height);
            const double ratio_x = static_cast<double>(width) / std::max(1, window_width);
            const double ratio_y = static_cast<double>(height) / std::max(1, window_height);
            const double mouse_y = event.motion.y * ratio_y;
            if (height <= top + bottom || mouse_y < top || mouse_y >= height - bottom) return;
            center_x -= event.motion.xrel * ratio_x / scale(height);
            center_y += event.motion.yrel * ratio_y / scale(height);
        }
    }

    void draw(const std::vector<Body>& bodies, const Config& config,
              std::size_t step, bool paused, double speed, const Trails& trails) {
        auto* render = renderer.get();
        int width, height;
        SDL_GetRendererOutputSize(render, &width, &height);
        if (height <= top + bottom || width <= 0) return;
        SDL_SetRenderDrawBlendMode(render, SDL_BLENDMODE_NONE);
        SDL_SetRenderDrawColor(render, 4, 7, 16, 255);
        SDL_RenderClear(render);
        const double pixels = scale(height);
        const double middle_y = top + (height - top - bottom) / 2.0;
        const auto px = [&](double x) { return width / 2.0 + (x - center_x) * pixels; };
        const auto py = [&](double y) { return middle_y - (y - center_y) * pixels; };
        // Acota proyecciones antes de convertir a entero, incluso con cuerpos
        // muy lejanos y zoom máximo. SDL recorta las líneas al viewport.
        const auto screen = [](double value) { return static_cast<int>(std::clamp(value, -1e6, 1e6)); };
        const SDL_Rect viewport{0, top, width, height - top - bottom};
        SDL_RenderSetClipRect(render, &viewport);

        // Fondo decorativo fijo, tenue y sin color de galaxia.
        for (const Star& star : background) {
            SDL_SetRenderDrawColor(render, star.brightness, star.brightness, star.brightness + 15, 255);
            SDL_RenderDrawPoint(render, static_cast<int>(star.x * width), top + static_cast<int>(star.y * viewport.h));
        }
        const double desired = 90 / pixels;
        const double magnitude = std::pow(10.0, std::floor(std::log10(desired)));
        double grid = magnitude;
        for (double factor : {1.0, 2.0, 5.0, 10.0}) {
            grid = factor * magnitude;
            if (grid >= desired) break;
        }
        if (show_grid) {
            SDL_SetRenderDrawColor(render, 19, 30, 44, 255);
            const double left = center_x - width / (2 * pixels), low = center_y - half_height;
            for (int tick = 0; tick <= static_cast<int>(std::ceil(width / pixels / grid)); ++tick) {
                const int x = screen(px((std::ceil(left / grid) + tick) * grid));
                SDL_RenderDrawLine(render, x, top, x, height - bottom);
            }
            for (int tick = 0; tick <= static_cast<int>(std::ceil(2 * half_height / grid)); ++tick) {
                const int y = screen(py((std::ceil(low / grid) + tick) * grid));
                SDL_RenderDrawLine(render, 0, y, width, y);
            }
        }
        if (show_trails && trails.frames.size() > 1) {
            SDL_SetRenderDrawBlendMode(render, SDL_BLENDMODE_ADD);
            for (std::size_t frame = 1; frame < trails.frames.size(); ++frame) {
                const auto alpha = static_cast<Uint8>(8 + 55 * frame / trails.frames.size());
                for (std::size_t i = 0; i < bodies.size(); ++i) {
                    const auto color = bodies[i].galaxy_id == 0 ? cyan : orange;
                    SDL_SetRenderDrawColor(render, color.r, color.g, color.b, alpha);
                    const auto& before = trails.frames[frame - 1][i];
                    const auto& after = trails.frames[frame][i];
                    SDL_RenderDrawLine(render, screen(px(before.x)), screen(py(before.y)),
                                       screen(px(after.x)), screen(py(after.y)));
                }
            }
        }

        std::size_t visible = 0;
        Vec2 centers[2]{};
        std::size_t counts[2]{};
        SDL_SetRenderDrawBlendMode(render, SDL_BLENDMODE_ADD);
        for (std::size_t i = 0; i < bodies.size(); ++i) {
            const Body& body = bodies[i];
            const int galaxy = body.galaxy_id == 0 ? 0 : 1;
            centers[galaxy].x += body.position.x; centers[galaxy].y += body.position.y;
            ++counts[galaxy];
            const double x = px(body.position.x), y = py(body.position.y);
            if (!(x >= -60 && x < width + 60 && y >= top - 60 && y < height - bottom + 60)) continue;
            if (x >= 0 && x < width && y >= top && y < height - bottom) ++visible;
            const SDL_Color color = galaxy == 0 ? cyan : orange;
            const int sx = screen(x), sy = screen(y);
            // Variación fija por índice: el brillo no parpadea al pausar ni buscar.
            const unsigned variation = static_cast<unsigned>((i * 2654435761u) % 100);
            halo(sx, sy, 34, color, 20);
            halo(sx, sy, 8 + variation / 25, color, 115);
            halo(sx, sy, 3, white, static_cast<Uint8>(170 + variation / 2));
            SDL_SetRenderDrawColor(render, 230, 242, 255, 220);
            SDL_RenderDrawPoint(render, sx, sy);
        }
        SDL_SetRenderDrawBlendMode(render, SDL_BLENDMODE_BLEND);
        // Etiquetas siguen el centro geométrico de cada población; no representan
        // estrellas centrales masivas ni se utilizan para calcular fuerzas.
        for (int galaxy = 0; galaxy < 2; ++galaxy) {
            if (counts[galaxy] == 0) continue;
            const double x = px(centers[galaxy].x / counts[galaxy]);
            const double y = py(centers[galaxy].y / counts[galaxy]);
            if (!(x >= 24 && x < width - 125 && y >= top + 20 && y < height - bottom - 25)) continue;
            const auto color = galaxy == 0 ? cyan : orange;
            SDL_SetRenderDrawColor(render, color.r, color.g, color.b, 90);
            SDL_RenderDrawLine(render, screen(x) + 12, screen(y) - 12, screen(x) + 28, screen(y) - 28);
            draw_text(render, galaxy == 0 ? "GALAXIA A" : "GALAXIA B", screen(x) + 32, screen(y) - 34, color, 2);
        }
        // Regla discreta con unidades físicas; reemplaza la rejilla por defecto.
        const int ruler = static_cast<int>(grid * pixels);
        SDL_SetRenderDrawColor(render, 110, 132, 156, 255);
        SDL_RenderDrawLine(render, 28, height - bottom - 24, 28 + ruler, height - bottom - 24);
        SDL_RenderDrawLine(render, 28, height - bottom - 28, 28, height - bottom - 20);
        SDL_RenderDrawLine(render, 28 + ruler, height - bottom - 28, 28 + ruler, height - bottom - 20);
        std::ostringstream ruler_text; ruler_text << std::setprecision(3) << grid << " U";
        draw_text(render, ruler_text.str(), 28, height - bottom - 42, muted, 1);
        SDL_RenderSetClipRect(render, nullptr);

        SDL_SetRenderDrawColor(render, 9, 15, 26, 255);
        SDL_Rect header{0, 0, width, top}, footer{0, height - bottom, width, bottom};
        SDL_RenderFillRect(render, &header); SDL_RenderFillRect(render, &footer);
        draw_text(render, "SIMULACIÓN DE GALAXIAS", 28, 22, white, 3);
        draw_text(render, "INTERACCION GRAVITACIONAL / PLANO XY", 29, 56, muted, 2);
        draw_text(render, "GALAXIA A", 29, 82, cyan, 2);
        draw_text(render, "GALAXIA B", 165, 82, orange, 2);
        const char* state = step == config.steps ? "FINALIZADA" : paused ? "EN PAUSA" : "REPRODUCIENDO";
        draw_text(render, state, width - 196, 26, cyan, 2);
        std::ostringstream status;
        status << "T " << std::fixed << std::setprecision(2) << step * config.dt << " / " << config.steps * config.dt;
        draw_text(render, status.str(), width - 196, 56, muted, 2);
        std::ostringstream info;
        info << bodies.size() << " CUERPOS  /  " << bodies.size() - visible << " FUERA DE VISTA";
        draw_text(render, info.str(), 335, 82, muted, 2);

        const auto bar = timeline(width, height);
        SDL_SetRenderDrawColor(render, 29, 43, 61, 255); SDL_RenderFillRect(render, &bar);
        const double progress = config.steps == 0 ? 1 : static_cast<double>(step) / config.steps;
        SDL_Rect fill{bar.x, bar.y, static_cast<int>(bar.w * progress), bar.h};
        SDL_SetRenderDrawColor(render, cyan.r, cyan.g, cyan.b, 255); SDL_RenderFillRect(render, &fill);
        std::ostringstream playback;
        playback << "FRAME " << step << '/' << config.steps << "   VELOCIDAD " << speed << " U/S";
        draw_text(render, playback.str(), 28, height - 64, white, 2);
        draw_text(render, "ESPACIO PAUSA   N FRAME   R INICIO   F ENCUADRAR   +/- VELOCIDAD", 28, height - 43, muted, width >= 1050 ? 2 : 1);
        draw_text(render, "T ESTELAS   G REJILLA   FLECHAS BUSCAR   RUEDA ZOOM   ARRASTRAR CAMARA   ESC SALIR", 28, height - 24, muted, width >= 1050 ? 2 : 1);
        SDL_RenderPresent(render);
    }
};
}  // namespace

std::size_t replay_native(TrajectoryReader& trajectory, bool close_when_complete) {
    const Config& config = trajectory.config();
    auto bodies = trajectory.read_frame(0);
    View view;
    view.fit(bodies);
    Trails trails(config.dt);
    trails.record(bodies, 0, true);
    std::size_t step = 0;
    bool paused = false;
    bool quit = false;
    double speed = 1.0;
    double accumulated = 0.0;
    auto previous = Clock::now();
    // Buscar reconstruye un historial limitado desde el propio archivo.
    const auto seek = [&](std::size_t target) {
        trails.frames.clear();
        const std::size_t span = 31 * trails.stride;
        const std::size_t first = target > span ? target - span : 0;
        for (std::size_t frame = first; frame < target;) {
            trails.record(trajectory.read_frame(frame), frame, true);
            if (target - frame <= trails.stride) break;
            frame += trails.stride;
        }
        bodies = trajectory.read_frame(target);
        trails.record(bodies, target, true);
        step = target;
        accumulated = 0;
    };

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
            if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                int width, height, window_width, window_height;
                SDL_GetRendererOutputSize(view.renderer.get(), &width, &height);
                SDL_GetWindowSize(view.window.get(), &window_width, &window_height);
                const double x = event.button.x * static_cast<double>(width) / std::max(1, window_width);
                const double y = event.button.y * static_cast<double>(height) / std::max(1, window_height);
                const auto bar = view.timeline(width, height);
                if (bar.w > 0 && y >= bar.y - 5 && y <= bar.y + bar.h + 5 &&
                    x >= bar.x && x <= bar.x + bar.w) {
                    const double fraction = (x - bar.x) / bar.w;
                    seek(std::min(config.steps, static_cast<std::size_t>(fraction * config.steps)));
                    paused = true;
                }
            }
            if (event.type != SDL_KEYDOWN || event.key.repeat) continue;
            switch (event.key.keysym.sym) {
            case SDLK_ESCAPE: quit = true; break;
            case SDLK_SPACE: paused = !paused; accumulated = 0; break;
            case SDLK_n: paused = true; single_step = true; accumulated = 0; break;
            case SDLK_f: view.fit(bodies); break;
            case SDLK_t: view.show_trails = !view.show_trails; break;
            case SDLK_g: view.show_grid = !view.show_grid; break;
            case SDLK_LEFT: {
                const auto jump = std::max<std::size_t>(1, config.steps / 100);
                seek(step > jump ? step - jump : 0); paused = true; break;
            }
            case SDLK_RIGHT: {
                const auto jump = std::max<std::size_t>(1, config.steps / 100);
                seek(step + std::min(jump, config.steps - step)); paused = true; break;
            }
            case SDLK_PLUS: case SDLK_EQUALS: case SDLK_KP_PLUS: speed = std::min(speed * 2, 16.0); break;
            case SDLK_MINUS: case SDLK_KP_MINUS: speed = std::max(speed / 2, 0.125); break;
            case SDLK_r:
                seek(0);
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

        // La reproducción lee estados ya guardados. Ningún control ejecuta
        // física ni modifica las métricas de la simulación.
        const auto batch_start = Clock::now();
        while (step < config.steps && (single_step || (!paused && accumulated >= config.dt))) {
            bodies = trajectory.read_frame(++step);
            trails.record(bodies, step);
            if (single_step) { single_step = false; break; }
            accumulated -= config.dt;
            // Conserva respuesta a eventos al leer trayectorias grandes.
            if (Clock::now() - batch_start > std::chrono::milliseconds(8)) break;
        }
        if (step == config.steps) paused = true;
        view.draw(bodies, config, step, paused, speed, trails);
        if (close_when_complete && step == config.steps) break;

        const double frame_ms = std::chrono::duration<double, std::milli>(Clock::now() - frame_start).count();
        if (frame_ms < 16.0) SDL_Delay(static_cast<Uint32>(16.0 - frame_ms));
    }
    return step;
}
}  // namespace nbody
