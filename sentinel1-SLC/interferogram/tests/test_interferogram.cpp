#include "interferogram/interferogram.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace interferogram;
int main() {
    try {
        const std::vector<Complex> a{{2,3}, {0,1}, {-2,3}, {0,0}};
        const std::vector<Complex> b{{4,5}, {0,1}, {4,-5}, {7,9}};
        const std::vector<Complex> expected{{23,2}, {1,0}, {-23,2}, {0,0}};
        std::vector<Complex> result(a.size());
        validateInterferogramInputs(a,b,result);
        computeInterferogramSequential(a,b,result);
        for (std::size_t i=0; i<a.size(); ++i)
            if (std::abs(result[i].real-expected[i].real)>1e-12 ||
                std::abs(result[i].imag-expected[i].imag)>1e-12)
                throw std::runtime_error("Resultado matemático incorrecto");
        bool rejected=false;
        try { validateInterferogramInputs(a, {}, result); }
        catch (const std::invalid_argument&) { rejected=true; }
        if (!rejected) throw std::runtime_error("Se aceptaron tamaños inválidos");
        std::cout << "PASS: kernel matemático y validación\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n'; return 1;
    }
}
