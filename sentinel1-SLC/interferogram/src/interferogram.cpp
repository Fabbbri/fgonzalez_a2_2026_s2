#include "interferogram/interferogram.hpp"
#include <stdexcept>

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
} 
