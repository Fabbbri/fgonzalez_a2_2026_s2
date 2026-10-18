#include "interferogram/interferogram.hpp"
#include <stdexcept>
#include <algorithm>

namespace interferogram {
/*
 * Comprueba que las entradas y la salida cumplan el contrato del kernel.
 *
 * La validación se realiza antes de medir el tiempo de cálculo.
 *
 * @param a Muestras complejas de la primera adquisición.
 * @param b Muestras complejas de la segunda adquisición.
 * @param result Vector de salida previamente reservado, distinto de a y b.
 *
 * @throws std::invalid_argument si algún vector está vacío, los tamaños
 *         difieren o la salida es el mismo vector que alguna entrada.
 */
void validateInterferogramInputs(const std::vector<Complex>& a,
                                const std::vector<Complex>& b,
                                const std::vector<Complex>& result) {
    if (a.empty() || a.size() != b.size() || a.size() != result.size() ||
        &a == &result || &b == &result)
        throw std::invalid_argument("El kernel requiere entradas no vacías y salida independiente del mismo tamaño.");
}

/*
 * Divide la ventana en tiles de tileWidth x tileHeight, recorriendo por filas.
 * Los tiles del borde derecho e inferior se recortan al tamaño restante, así
 * que cada píxel pertenece exactamente a un tile.
 *
 * @throws std::invalid_argument si alguna dimensión no es positiva.
 */
std::vector<Tile> makeTiles(int width, int height, int tileWidth, int tileHeight) {
    if (width <= 0 || height <= 0 || tileWidth <= 0 || tileHeight <= 0)
        throw std::invalid_argument("Las dimensiones de ventana y tile deben ser positivas.");
    std::vector<Tile> tiles;
    for (int y = 0; y < height;) {
        const int yEnd = y + std::min(tileHeight, height - y);
        for (int x = 0; x < width;) {
            const int xEnd = x + std::min(tileWidth, width - x);
            tiles.push_back({x, y, xEnd, yEnd});
            x = xEnd;
        }
        y = yEnd;
    }
    return tiles;
}

} // namespace interferogram
