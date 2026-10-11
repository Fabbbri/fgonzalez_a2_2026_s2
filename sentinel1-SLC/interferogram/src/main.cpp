#include "interferogram/config.hpp"
#include "interferogram/slc_reader.hpp"
#include "interferogram/interferogram.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <stdexcept>

/*
 * Coordina la configuración, la lectura de las adquisiciones y el cálculo.
 *
 * @param argc Número de argumentos recibidos.
 * @param argv Arreglo de argumentos de la línea de comandos.
 * @return 0 al completar el cálculo o mostrar ayuda; 1 ante un error capturado.
 */
int main(int argc, char* argv[]) {
    using namespace interferogram;
    try {
        const Config config = parseConfig(argc, argv);
        if (config.help) {
            printHelp(std::cout);
            return 0;
        }
        // Usar las mismas coordenadas en A y B; esto no realiza corregistro.
        const auto a = readSlcWindow(config.a, config.x, config.y, config.width, config.height);
        const auto b = readSlcWindow(config.b, config.x, config.y, config.width, config.height);
        // Preparar y validar la salida antes de iniciar el cronómetro.
        std::vector<Complex> result(a.size());
        validateInterferogramInputs(a, b, result);

        // El reloj monótono mide solo el kernel, excluyendo lectura e impresión.
        const auto start = std::chrono::steady_clock::now();
        computeInterferogramSequential(a, b, result);
        const auto stop = std::chrono::steady_clock::now();
        const double milliseconds =
            std::chrono::duration<double, std::milli>(stop - start).count();

        // Resumen numérico para comparar ejecuciones, no una medida de calidad física.
        Complex sum{0, 0};
        for (const auto& value : result) {
            sum.real += value.real;
            sum.imag += value.imag;
        }
        std::cout << "P09-R2\n"
                  << "A: " << config.a << "\nB: " << config.b << '\n'
                  << "Window: " << config.width << " x " << config.height << '\n'
                  << "Offset: " << config.x << ", " << config.y << '\n'
                  << "Samples: " << a.size() << "\nMode: sequential\n"
                  << std::fixed << std::setprecision(6)
                  << "Kernel time: " << milliseconds << " ms\n"
                  << std::setprecision(0)
                  << "Result sum (real, imag): " << sum.real << ", " << sum.imag << '\n'
                  << "Computational kernel; coregistration quality is not verified by this executable.\n";
        return 0;
    // Convertir errores de configuración, lectura o cálculo en un fallo de la CLI.
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
