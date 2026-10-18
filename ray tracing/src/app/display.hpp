#pragma once
#include "render/image.hpp"
#include "render/tiles.hpp"
#include <memory>

namespace rt {
// La presentación desconoce esferas, cámara e iluminación.
class Display {
public:
    Display(int width, int height);
    ~Display();
    bool poll();
    void present(const Image& image, const Tile* changed = nullptr);
    void wait_until_closed();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace rt
