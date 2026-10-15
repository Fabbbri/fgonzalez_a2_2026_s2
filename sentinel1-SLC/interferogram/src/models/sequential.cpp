#include "interferogram/interferogram.hpp"
#include "interferogram/kernel.hpp"

namespace interferogram {
/*
 * Calcula el producto de cada muestra de a por el conjugado de b.
 *
 * Recorre N muestras en tiempo O(N), sin reservar memoria ni validar argumentos.
 * Las entradas se conservan y se sobrescriben las componentes de la salida.
 *
 * @param a Muestras complejas de la primera adquisición.
 * @param b Muestras complejas de la segunda adquisición.
 * @param result Vector donde se almacena el producto, posición por posición.
 */
void computeInterferogramSequential(const std::vector<Complex>& a,
    const std::vector<Complex>& b, std::vector<Complex>& result) {
    // Cada iteración utiliza solo la posición i: puede repartirse entre hilos.
    // (ar + ai*i) * (br - bi*i) = (ar*br + ai*bi) + (ai*br - ar*bi)*i.
    // En la fórmula, i representa la unidad imaginaria; en el ciclo, el índice.
    for (std::size_t i = 0; i < a.size(); ++i) {
        result[i] = conjugateProduct(a[i], b[i]);
    }
}
} // namespace interferogram
