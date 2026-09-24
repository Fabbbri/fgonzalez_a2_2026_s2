#pragma once

namespace interferogram {
/**
 * Representa una muestra compleja SLC mediante dos componentes de doble precisión.
 * Mantener ambas componentes permite calcular el producto con el conjugado.
 */
struct Complex {
    double real; // Componente real de la muestra.
    double imag; // Coeficiente de la unidad imaginaria.
};
} 
