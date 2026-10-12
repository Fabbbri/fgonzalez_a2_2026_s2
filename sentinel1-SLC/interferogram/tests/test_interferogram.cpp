#include "interferogram/interferogram.hpp"
#include "interferogram/models/fine_grained.hpp"
#include "interferogram/models/coarse_grained.hpp"
#include "interferogram/models/smt.hpp"
#include "interferogram/models/cmp.hpp"
#ifdef __linux__
#include <sched.h>
#endif
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
            if (tiles.size() != static_cast<std::size_t>(expectedTileCount)) {
                throw std::runtime_error("Cantidad de tiles incorrecta");
            }
            std::vector<int> visits(width * height, 0);
            for (const auto& tile : tiles) {
                if (tile.xBegin < 0 || tile.yBegin < 0 || tile.xEnd > width ||
                    tile.yEnd > height || tile.xEnd <= tile.xBegin || tile.yEnd <= tile.yBegin) {
                    throw std::runtime_error("Tile fuera de límites");
                }
                for (int y = tile.yBegin; y < tile.yEnd; ++y) {
                    for (int x = tile.xBegin; x < tile.xEnd; ++x) {
                        ++visits[y * width + x];
                    }
                }
            }
            // NaN permite detectar si quedó alguna muestra sin escribir.
            const double nan = std::numeric_limits<double>::quiet_NaN();
            std::vector<Complex> result(a.size(), {nan, nan});
            computeInterferogramTiled(a, b, result, width, tiles);
            for (std::size_t i = 0; i < a.size(); ++i) {
                if (visits[i] != 1) {
                    throw std::runtime_error("Cada píxel debe pertenecer a un solo tile");
                }
                if (result[i].real != expected[i].real || result[i].imag != expected[i].imag) {
                    throw std::runtime_error("El cálculo por tiles no coincide con el secuencial");
                }
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
            if (!rejected) {
                throw std::runtime_error("Dimensión de tile inválida aceptada");
            }
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
        if (!(realError <= 1e-12 && imagError <= 1e-12)) {
            throw std::runtime_error("Resultado matemático incorrecto");
        }
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
    if (!rejected) {
        throw std::runtime_error("Se aceptó una segunda entrada vacía");
    }
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
                    scheduler.dispatches() != 0 || scheduler.finished()) {
                    throw std::runtime_error("Estado inicial Fine-Grained incorrecto");
                }
                while (!scheduler.finished()) {
                    if (++round > a.size()) {
                        throw std::runtime_error("Fine-Grained no termina");
                    }
                    scheduler.runRound(a, b, result);
                    for (const auto& tile : tiles) {
                        const int count = (tile.xEnd - tile.xBegin) * (tile.yEnd - tile.yBegin);
                        if ((round - 1) * quantum < static_cast<std::size_t>(count)) {
                            ++expectedDispatches;
                        }
                        int local = 0;
                        for (int y = tile.yBegin; y < tile.yEnd; ++y) {
                            for (int x = tile.xBegin; x < tile.xEnd; ++x, ++local) {
                                const int i = y * width + x;
                                if (static_cast<std::size_t>(local) < round * quantum) {
                                    if (result[i].real != expected[i].real || result[i].imag != expected[i].imag) {
                                        throw std::runtime_error("Progreso Fine-Grained incorrecto");
                                    }
                                } else if (!std::isnan(result[i].real) || !std::isnan(result[i].imag)) {
                                    throw std::runtime_error("Una tarea excedió su quantum");
                                }
                            }
                        }
                    }
                    if (scheduler.rounds() != round || scheduler.dispatches() != expectedDispatches) {
                        throw std::runtime_error("Estadísticas Fine-Grained incorrectas");
                    }
                }
                // Una llamada después del final no debe volver a ejecutar tareas.
                scheduler.runRound(a, b, result);
                if (scheduler.rounds() != round || scheduler.dispatches() != expectedDispatches) {
                    throw std::runtime_error("Se ejecutaron tareas ya terminadas");
                }
                for (std::size_t i = 0; i < a.size(); ++i) {
                    if (result[i].real != expected[i].real || result[i].imag != expected[i].imag) {
                        throw std::runtime_error("Fine-Grained difiere del secuencial");
                    }
                }
            }
        }
    }
    for (int quantum : {0, -1}) {
        bool rejected = false;
        try {
            FineGrained scheduler(7, 5, 3, 2, quantum);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        if (!rejected) {
            throw std::runtime_error("Quantum inválido aceptado");
        }
    }
}

void testCoarseGrained() {
    for (int tileSize : {1, 3, 20}) {
        for (int workers : {1, 2, 8}) {
            CoarseGrained pool(7, 5, tileSize, tileSize, workers);
            std::vector<Complex> a(35), b(35), expected(35), result(35);
            // Cambiar entradas en cada generación comprueba la reutilización del pool.
            for (int run = 0; run < 30; ++run) {
                for (int i = 0; i < 35; ++i) {
                    a[i] = {i * 0.25 + run, -i * 0.5};
                    b[i] = {i - 4.0, run * 0.125};
                    result[i] = {NAN, NAN};
                }
                computeInterferogramSequential(a, b, expected);
                pool.compute(a, b, result);
                for (int i = 0; i < 35; ++i) {
                    if (result[i].real != expected[i].real || result[i].imag != expected[i].imag) {
                        throw std::runtime_error("Coarse-Grained difiere del secuencial");
                    }
                }
            }
            bool rejected = false;
            try {
                pool.compute(a, {}, result);
            } catch (const std::invalid_argument&) {
                rejected = true;
            }
            if (!rejected) {
                throw std::runtime_error("Pool aceptó entradas inválidas");
            }
            pool.compute(a, b, result);
        }
    }
    {
        CoarseGrained idle(1, 1, 1, 1, 2);
    } // Destruir sin publicar trabajo.
    for (int workers : {0, -1}) {
        bool rejected = false;
        try {
            CoarseGrained pool(7, 5, 3, 3, workers);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        if (!rejected) {
            throw std::runtime_error("Cantidad de trabajadores inválida aceptada");
        }
    }
}

void testSmt() {
    if (selectSmtWorkers(0, 8) != 16 || selectSmtWorkers(9, 8) != 9) {
        throw std::runtime_error("Selección de hilos SMT incorrecta");
    }
    for (int count : {-1, 1, 8}) {
        bool rejected = false;
        try {
            selectSmtWorkers(count, 8);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        if (!rejected) {
            throw std::runtime_error("SMT aceptó una cantidad sin sobresuscripción");
        }
    }
    bool rejected = false;
    try {
        selectSmtWorkers(0, 0);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    if (!rejected) {
        throw std::runtime_error("SMT aceptó una topología vacía");
    }
#ifdef __linux__
    // Una CPU permitida basta para probar sobresuscripción sin crear cientos de hilos.
    struct RestoreAffinity {
        cpu_set_t original;
        ~RestoreAffinity() {
            sched_setaffinity(0, sizeof(original), &original);
        }
    } affinity;
    if (sched_getaffinity(0, sizeof(affinity.original), &affinity.original) != 0) {
        throw std::runtime_error("No se pudo leer la afinidad de prueba");
    }
    const auto originalTopology = detectCpuInfo();
    cpu_set_t single;
    CPU_ZERO(&single);
    CPU_SET(originalTopology.allowedCpus.front(), &single);
    if (sched_setaffinity(0, sizeof(single), &single) != 0) {
        throw std::runtime_error("No se pudo restringir la afinidad de prueba");
    }
    for (int tileSize : {1, 3, 20}) {
        Smt model(7, 5, tileSize, tileSize);
        if (model.workers() != 2 || model.topology().physicalCores != 1 ||
            model.topology().allowedCpus.size() != 1) {
            throw std::runtime_error("SMT no respeta los núcleos disponibles");
        }
        std::vector<Complex> a(35), b(35), expected(35), result(35);
        for (int run = 0; run < 10; ++run) {
            for (int i = 0; i < 35; ++i) {
                a[i] = {i * 0.25 - run, i * -0.5};
                b[i] = {run * 0.125, i - 3.0};
                result[i] = {NAN, NAN};
            }
            computeInterferogramSequential(a, b, expected);
            model.compute(a, b, result);
            for (int i = 0; i < 35; ++i) {
                if (result[i].real != expected[i].real || result[i].imag != expected[i].imag) {
                    throw std::runtime_error("SMT difiere del secuencial");
                }
            }
        }
    }
    rejected = false;
    try {
        Smt invalid(7, 5, 3, 3, 1);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    if (!rejected) {
        throw std::runtime_error("SMT permitió un hilo en un núcleo disponible");
    }
#endif
}

// Ventana no divisible entre los trabajadores y reutilización del mismo pool.
void testCmp() {
#ifdef __linux__
    const auto info = detectCpuInfo();
    for (int workers : {1, static_cast<int>(std::min<std::size_t>(3, info.physicalCores))}) {
        Cmp model(7, 5, workers);
        if (model.workers() != static_cast<std::size_t>(workers)) {
            throw std::runtime_error("Cantidad incorrecta de trabajadores CMP");
        }
        for (int worker = 0; worker < workers; ++worker) {
            if (model.cpus()[worker] != info.coreCpus[worker]) {
                throw std::runtime_error("CMP no seleccionó núcleos distintos");
            }
        }
        std::vector<Complex> a(35), b(35), expected(35), result(35);
        for (int run = 0; run < 10; ++run) {
            for (int i = 0; i < 35; ++i) {
                a[i] = {i * 0.25 - run, i * -0.5};
                b[i] = {run * 0.125, i - 3.0};
                result[i] = {NAN, NAN};
            }
            computeInterferogramSequential(a, b, expected);
            model.compute(a, b, result);
            for (int i = 0; i < 35; ++i) {
                if (result[i].real != expected[i].real || result[i].imag != expected[i].imag) {
                    throw std::runtime_error("CMP difiere del secuencial");
                }
            }
        }
        bool rejected = false;
        try {
            std::vector<Complex> small(1);
            model.compute(small, small, small);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        if (!rejected) {
            throw std::runtime_error("CMP aceptó una ventana diferente");
        }
    }
    Cmp automatic(1, 1);
    if (automatic.workers() != 1) {
        throw std::runtime_error("CMP creó trabajadores sin muestras");
    }
    for (int count : {-1, static_cast<int>(info.physicalCores + 1)}) {
        bool rejected = false;
        try {
            Cmp invalid(7, 5, count);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        if (!rejected) {
            throw std::runtime_error("CMP aceptó hilos inválidos");
        }
    }
#endif
}

int main() {
    try {
        testFineGrained();
        testCoarseGrained();
        testSmt();
        testCmp();
        testConjugateProduct();
        testInvalidInputs();
        testTiles();
        testInvalidTileDimensions();
        std::cout << "PASS: kernel, tiles, Fine-Grained, Coarse-Grained, SMT, CMP, cobertura y validación\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
