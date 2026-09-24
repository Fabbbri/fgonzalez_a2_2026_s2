#include "interferogram/config.hpp"
#include "interferogram/slc_reader.hpp"
#include "interferogram/interferogram.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) {
    using namespace interferogram;
    try {
        const Config config = parseConfig(argc, argv);
        if (config.help) {
            printHelp(std::cout);
            return 0;
        }
        const auto a = readSlcWindow(config.a, config.x, config.y, config.width, config.height);
        const auto b = readSlcWindow(config.b, config.x, config.y, config.width, config.height);
        std::vector<Complex> result(a.size());
        validateInterferogramInputs(a, b, result);

        const auto start = std::chrono::steady_clock::now();
        computeInterferogramSequential(a, b, result);
        const auto stop = std::chrono::steady_clock::now();
        const double milliseconds =
            std::chrono::duration<double, std::milli>(stop - start).count();

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
                  << "Computational kernel; pixelwise coregistration not established.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
