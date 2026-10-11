#include "interferogram/interferogram.hpp"
#include "interferogram/models/fine_grained.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <limits>

using namespace interferogram;
// Probar una ventana rectangular ayuda a detectar errores al recorrer filas y bordes.
void testTiles() {
    const int width = 7;
    const int height = 5;
    std::vector<Complex> a(width * height);
    std::vector<Complex> b(width * height);
    std::vector<Complex> expected(width * height);
    for (int i = 0; i < width * height; ++i) {
        a[i] = {i * 0.25 - 3, i * -0.5 + 2};
        b[i] = {i * -0.125 + 1, i * 0.75 - 4};
    }
    computeInterferogramSequential(a, b, expected);
    for (int tileWidth : {1, 3, 7, 20}) {
        for (int tileHeight : {1, 2, 5, 20}) {
            const auto tiles = makeTiles(width, height, tileWidth, tileHeight);
            const int columns = (width + tileWidth - 1) / tileWidth;
            const int rows = (height + tileHeight - 1) / tileHeight;
            const int expectedTileCount = columns * rows;
            if (tiles.size() != static_cast<std::size_t>(expectedTileCount))
                throw std::runtime_error("Cantidad de tiles incorrecta");
            std::vector<int> visits(width * height, 0);
            for (const auto& tile : tiles) {
                if (tile.xBegin < 0 || tile.yBegin < 0 || tile.xEnd > width ||
                    tile.yEnd > height || tile.xEnd <= tile.xBegin || tile.yEnd <= tile.yBegin)
                    throw std::runtime_error("Tile fuera de límites");
                for (int y = tile.yBegin; y < tile.yEnd; ++y)
                    for (int x = tile.xBegin; x < tile.xEnd; ++x)
                        ++visits[y * width + x];
            }
            // NaN permite detectar si quedó alguna muestra sin escribir.
            const double nan = std::numeric_limits<double>::quiet_NaN();
            std::vector<Complex> result(a.size(), {nan, nan});
            computeInterferogramTiled(a, b, result, width, tiles);
            for (std::size_t i = 0; i < a.size(); ++i) {
                if (visits[i] != 1)
                    throw std::runtime_error("Cada píxel debe pertenecer a un solo tile");
                if (result[i].real != expected[i].real || result[i].imag != expected[i].imag)
                    throw std::runtime_error("El cálculo por tiles no coincide con el secuencial");
            }
        }
    }
}

void testInvalidTileDimensions() {
    for (int invalidSize : {0, -1}) {
        for (int dimension = 0; dimension < 4; ++dimension) {
            int dims[]{7, 5, 3, 2};
            dims[dimension] = invalidSize;
            bool rejected = false;
            try {
                makeTiles(dims[0], dims[1], dims[2], dims[3]);
            } catch (const std::invalid_argument&) {
                rejected = true;
            }
            if (!rejected)
                throw std::runtime_error("Dimensión de tile inválida aceptada");
        }
    }
}

// Casos calculados a mano para comprobar signos, unidad imaginaria y cero.
void testConjugateProduct() {
    const std::vector<Complex> a{{2, 3}, {0, 1}, {-2, 3}, {0, 0}};
    const std::vector<Complex> b{{4, 5}, {0, 1}, {4, -5}, {7, 9}};
    const std::vector<Complex> expected{{23, 2}, {1, 0}, {-23, 2}, {0, 0}};
    std::vector<Complex> result(a.size());
    validateInterferogramInputs(a, b, result);
    computeInterferogramSequential(a, b, result);

    for (std::size_t i = 0; i < a.size(); ++i) {
        const double realError = std::abs(result[i].real - expected[i].real);
        const double imagError = std::abs(result[i].imag - expected[i].imag);
        if (!(realError <= 1e-12 && imagError <= 1e-12))
            throw std::runtime_error("Resultado matemático incorrecto");
    }
}

void testInvalidInputs() {
    const std::vector<Complex> a{{2, 3}};
    std::vector<Complex> result(a.size());
    bool rejected = false;
    try {
        validateInterferogramInputs(a, {}, result);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    if (!rejected)
        throw std::runtime_error("Se aceptó una segunda entrada vacía");
}

void testFineGrained() {
    const int width = 7, height = 5;
    std::vector<Complex> a(width * height), b(width * height), expected(width * height);
    for (int i = 0; i < width * height; ++i) {
        a[i] = {0.25 * i - 3, 0.5 * i};
        b[i] = {2 - 0.125 * i, -0.75 * i};
    }
    computeInterferogramSequential(a, b, expected);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (int tileSize : {1, 3, 20}) {
        const auto tiles = makeTiles(width, height, tileSize, tileSize);
        for (int quantum : {1, 2, 4, 100}) {
            FineGrained scheduler(width, height, tileSize, tileSize, quantum);
            // Reutilizar el mismo planificador comprueba que reset restaure los contextos.
            for (int repetition = 0; repetition < 2; ++repetition) {
                scheduler.reset();
                std::vector<Complex> result(a.size(), {nan, nan});
                std::size_t expectedDispatches = 0;
                std::size_t round = 0;
                if (scheduler.tasks() != tiles.size() || scheduler.rounds() != 0 ||
                    scheduler.dispatches() != 0 || scheduler.finished())
                    throw std::runtime_error("Estado inicial Fine-Grained incorrecto");
                while (!scheduler.finished()) {
                    if (++round > a.size())
                        throw std::runtime_error("Fine-Grained no termina");
                    scheduler.runRound(a, b, result);
                    for (const auto& tile : tiles) {
                        const int count = (tile.xEnd - tile.xBegin) * (tile.yEnd - tile.yBegin);
                        if ((round - 1) * quantum < static_cast<std::size_t>(count))
                            ++expectedDispatches;
                        int local = 0;
                        for (int y = tile.yBegin; y < tile.yEnd; ++y) {
                            for (int x = tile.xBegin; x < tile.xEnd; ++x, ++local) {
                                const int i = y * width + x;
                                if (static_cast<std::size_t>(local) < round * quantum) {
                                    if (result[i].real != expected[i].real || result[i].imag != expected[i].imag)
                                        throw std::runtime_error("Progreso Fine-Grained incorrecto");
                                } else if (!std::isnan(result[i].real) || !std::isnan(result[i].imag)) {
                                    throw std::runtime_error("Una tarea excedió su quantum");
                                }
                            }
                        }
                    }
                    if (scheduler.rounds() != round || scheduler.dispatches() != expectedDispatches)
                        throw std::runtime_error("Estadísticas Fine-Grained incorrectas");
                }
                // Una llamada después del final no debe volver a ejecutar tareas.
                scheduler.runRound(a, b, result);
                if (scheduler.rounds() != round || scheduler.dispatches() != expectedDispatches)
                    throw std::runtime_error("Se ejecutaron tareas ya terminadas");
                for (std::size_t i = 0; i < a.size(); ++i)
                    if (result[i].real != expected[i].real || result[i].imag != expected[i].imag)
                        throw std::runtime_error("Fine-Grained difiere del secuencial");
            }
        }
    }
    for (int quantum : {0, -1}) {
        bool rejected = false;
        try { FineGrained scheduler(7, 5, 3, 2, quantum); }
        catch (const std::invalid_argument&) { rejected = true; }
        if (!rejected) throw std::runtime_error("Quantum inválido aceptado");
    }
}

int main() {
    try {
        testFineGrained();
        testConjugateProduct();
        testInvalidInputs();
        testTiles();
        testInvalidTileDimensions();
        std::cout << "PASS: kernel, tiles, Fine-Grained, cobertura y validación\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
