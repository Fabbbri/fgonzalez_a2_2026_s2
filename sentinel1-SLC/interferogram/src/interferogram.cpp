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
 * Calcula el producto de cada muestra de a por el conjugado de b.
 *
 * Recorre N muestras en tiempo O(N), sin reservar memoria ni validar argumentos.
 * Las entradas se conservan y se sobrescriben las componentes de la salida.
 *
 * @param a      Muestras complejas de la primera adquisición.
 * @param b      Muestras complejas de la segunda adquisición.
 * @param result Vector donde se almacena el producto, posición por posición.
 */
void computeInterferogramSequential(const std::vector<Complex>& a,
                                   const std::vector<Complex>& b,
                                   std::vector<Complex>& result) {
    // Cada iteración utiliza solo la posición i: puede repartirse entre hilos.
    // (ar + ai*i) * (br - bi*i) = (ar*br + ai*bi) + (ai*br - ar*bi)*i.
    // En la fórmula, i representa la unidad imaginaria; en el ciclo, el índice.
    for (std::size_t i = 0; i < a.size(); ++i) {
        result[i].real = a[i].real * b[i].real + a[i].imag * b[i].imag;
        result[i].imag = a[i].imag * b[i].real - a[i].real * b[i].imag;
    }
}
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

void computeInterferogramTiled(const std::vector<Complex>& a,
                              const std::vector<Complex>& b,
                              std::vector<Complex>& result,
                              int width, const std::vector<Tile>& tiles) {
    // Cada tile escribe únicamente en su región de la salida.
    for (const Tile& tile : tiles) {
        for (int y = tile.yBegin; y < tile.yEnd; ++y) {
            for (int x = tile.xBegin; x < tile.xEnd; ++x) {
                const std::size_t i = static_cast<std::size_t>(y) * width + x;
                result[i].real = a[i].real * b[i].real + a[i].imag * b[i].imag;
                result[i].imag = a[i].imag * b[i].real - a[i].real * b[i].imag;
            }
        }
    }
}
} 
