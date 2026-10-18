/**
 * @file display.cpp
 * Gestión de ventana, textura RGB y eventos en el hilo principal.
 * Los trabajadores calculan píxeles en CPU; SDL únicamente presenta el buffer.
 * La implementación alternativa sin SDL conserva la interfaz y rechaza ventanas.
 */
#include "app/display.hpp"
#include <stdexcept>
#include <string>

#ifdef RT_WITH_SDL
#include <SDL.h>

namespace rt {
namespace {
// Convierte un resultado de SDL en excepción con el diagnóstico de la biblioteca.
void check(bool success) {
    if (!success) throw std::runtime_error(std::string("SDL2: ") + SDL_GetError());
}
}
// Estado privado de presentación; la cabecera pública no depende de SDL.
struct Display::Impl {
    // Recursos SDL iniciados en nulo: una construcción parcial sigue siendo liberable.
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    bool open = true;
    // Destrucción RAII en orden inverso: textura, renderizador, ventana y subsistema.
    ~Impl() {
        if (texture) SDL_DestroyTexture(texture);
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        SDL_Quit();
    }
    // Limpia el destino, copia la textura completa y presenta el frame.
    void redraw() {
        check(SDL_RenderClear(renderer) == 0);
        check(SDL_RenderCopy(renderer, texture, nullptr, nullptr) == 0);
        SDL_RenderPresent(renderer);
    }
    // Procesa cierre, Escape y exposición de ventana en el coordinador.
    void event(const SDL_Event& e) {
        if (e.type == SDL_QUIT ||
            (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) ||
            (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_CLOSE))
            open = false;
        if (open && e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_EXPOSED)
            redraw();
    }
};
// Crea recursos; make_unique garantiza liberación incluso si falla una etapa posterior.
Display::Display(int width, int height) : impl_(std::make_unique<Impl>()) {
    // Inicializa únicamente el subsistema de video.
    check(SDL_Init(SDL_INIT_VIDEO) == 0);
    // Ventana centrada con dimensiones de imagen.
    impl_->window = SDL_CreateWindow("Ray tracer (Esc para cerrar)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN);
    check(impl_->window != nullptr);
    // Software y sin vsync: SDL presenta píxeles; no calcula la escena 3D.
    impl_->renderer = SDL_CreateRenderer(impl_->window, -1, SDL_RENDERER_SOFTWARE);
    check(impl_->renderer != nullptr);
    // Textura actualizable RGB24; corresponde a tres bytes contiguos por píxel.
    impl_->texture = SDL_CreateTexture(impl_->renderer, SDL_PIXELFORMAT_RGB24,
        SDL_TEXTUREACCESS_STREAMING, width, height);
    check(impl_->texture != nullptr);
}
// La destrucción delega al unique_ptr y al destructor de Impl.
Display::~Display() = default;
// Drena eventos disponibles sin bloquear y devuelve si la ventana continúa abierta.
bool Display::poll() {
    SDL_Event e;
    while (SDL_PollEvent(&e)) impl_->event(e);
    return impl_->open;
}
void Display::present(const Image& image, const Tile* changed) {
    // Rectángulo opcional de actualización parcial; solo se usa cuando changed existe.
    SDL_Rect rect;
    // Puntero de lectura al comienzo del buffer ya calculado.
    const Pixel* pixels = image.data();
    // Ajusta origen y dimensiones al tile terminado que se quiere transferir.
    if (changed) {
        rect = {changed->x0, changed->y0, changed->x1 - changed->x0, changed->y1 - changed->y0};
        // Desplazamiento por filas completas hasta el primer píxel de la región.
        pixels += static_cast<std::size_t>(changed->y0) * image.width() + changed->x0;
    }
    // El pitch sigue siendo W*3 incluso al actualizar un tile: distancia entre filas
    // en la imagen fuente. La copia SDL no calcula rayos ni introduce paralelismo.
    check(SDL_UpdateTexture(impl_->texture, changed ? &rect : nullptr,
                            pixels, image.width() * 3) == 0);
    impl_->redraw();
}
void Display::wait_until_closed() {
    // Una vez terminado, esperar eventos evita consumir CPU en un bucle vacío.
    SDL_Event e;
    // Tras terminar el cálculo, espera eventos sin sondeo continuo.
    while (impl_->open) {
        check(SDL_WaitEvent(&e) != 0);
        impl_->event(e);
    }
}
} // namespace rt
// Compilación alternativa: mantiene los símbolos públicos sin enlazar SDL.
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
