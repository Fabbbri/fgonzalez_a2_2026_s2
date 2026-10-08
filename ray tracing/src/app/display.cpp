#include "app/display.hpp"
#include <stdexcept>
#include <string>

#ifdef RT_WITH_SDL
#include <SDL.h>

namespace rt {
namespace {
void check(bool success) {
    if (!success) throw std::runtime_error(std::string("SDL2: ") + SDL_GetError());
}
}
struct Display::Impl {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    bool open = true;
    ~Impl() {
        if (texture) SDL_DestroyTexture(texture);
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }
    void redraw() {
        check(SDL_RenderClear(renderer) == 0);
        check(SDL_RenderCopy(renderer, texture, nullptr, nullptr) == 0);
        SDL_RenderPresent(renderer);
    }
    void event(const SDL_Event& e) {
        if (e.type == SDL_QUIT ||
            (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) ||
            (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE))
            open = false;
        if (open && e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_EXPOSED)
            redraw();
    }
};
Display::Display(int width, int height) : impl_(std::make_unique<Impl>()) {
    check(SDL_Init(SDL_INIT_VIDEO) == 0);
    impl_->window = SDL_CreateWindow("Ray tracer (Esc para cerrar)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN);
    check(impl_->window != nullptr);
    // Software y sin vsync: SDL presenta píxeles; no calcula la escena 3D.
    impl_->renderer = SDL_CreateRenderer(impl_->window, -1, SDL_RENDERER_SOFTWARE);
    check(impl_->renderer != nullptr);
    impl_->texture = SDL_CreateTexture(impl_->renderer, SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STREAMING, width, height);
    check(impl_->texture != nullptr);
}
Display::~Display() = default;
bool Display::poll() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) impl_->event(e);
    return impl_->open;
}
void Display::present(const Image& image, const Tile* changed) {
    SDL_Rect rect;
    const Pixel* pixels = image.data();
    if (changed) {
        rect = {changed->x0, changed->y0, changed->x1 - changed->x0, changed->y1 - changed->y0};
        pixels += static_cast<std::size_t>(changed->y0) * image.width() + changed->x0;
    }
    check(SDL_UpdateTexture(impl_->texture, changed ? &rect : nullptr,
                            pixels, image.width() * 3) == 0);
    impl_->redraw();
}
void Display::wait_until_closed() {
    // Una vez terminado, esperar eventos evita consumir CPU en un bucle vacío.
    SDL_Event e;
    while (impl_->open) {
        check(SDL_WaitEvent(&e) != 0);
        impl_->event(e);
    }
}
} // namespace rt
#else
namespace rt {
struct Display::Impl {};
Display::Display(int, int) {
    throw std::runtime_error("Compilación sin SDL2: usa --headless o compila WITH_SDL=1");
}
Display::~Display() = default;
bool Display::poll() { return false; }
void Display::present(const Image&, const Tile*) {}
void Display::wait_until_closed() {}
} // namespace rt
#endif
