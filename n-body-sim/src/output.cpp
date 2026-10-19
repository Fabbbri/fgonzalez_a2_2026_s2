/**
 * @file output.cpp
 * Presentación del diagnóstico físico de la interfaz secuencial anterior.
 * Compara energía, momento y movimiento esperado del centro de masa.
 * No se enlaza en el ejecutable multihilo construido por el Makefile actual.
 */
#include "nbody/output.hpp"
#include <cmath>
#include <iomanip>
#include <ostream>

namespace nbody {
// Salida descriptiva del estado inicial/final; los cambios se calculan fuera de la integración.
void write_physics_report(const Diagnostics& initial, const Diagnostics& final,
                          double time, std::ostream& output) {
    // Diferencia E_final-E_inicial con signo.
    const double delta_energy = final.energy() - initial.energy();
    // Norma euclidiana del cambio de momento para comparar componentes conjuntamente.
    const double delta_momentum = std::hypot(final.momentum.x - initial.momentum.x,
                                              final.momentum.y - initial.momentum.y);
    // El centro de masa puede desplazarse: esperamos R(t) = R(0) + P(0)/M*t.
    // Movimiento inercial esperado R0+(P0/M)*t en ausencia de fuerza externa.
    const Vec2 expected_center{
        initial.center_of_mass.x + initial.momentum.x / initial.mass * time,
        initial.center_of_mass.y + initial.momentum.y / initial.mass * time};
    // Norma de la diferencia entre centro calculado y predicción inercial.
    const double center_error = std::hypot(final.center_of_mass.x - expected_center.x,
                                            final.center_of_mass.y - expected_center.y);

    // Usa 17 dígitos significativos para conservar precisión de las magnitudes double.
    output << std::setprecision(17)
           << "Diagnóstico físico (inicial -> final; fuera del cronómetro):\n"
           << "  Masa: " << initial.mass << " -> " << final.mass << '\n'
           << "  Energía cinética: " << initial.kinetic_energy << " -> " << final.kinetic_energy << '\n'
           << "  Energía potencial: " << initial.potential_energy << " -> " << final.potential_energy << '\n'
           << "  Energía total: " << initial.energy() << " -> " << final.energy() << '\n'
           << "  Cambio de energía: " << delta_energy << '\n'
           << "  Cambio relativo de energía respecto a |E inicial|: ";
    // Evita dividir entre cero al informar cambio relativo de energía.
    if (initial.energy() == 0.0) {
        output << "no definido (energía inicial cero)\n";
    } else {
        output << delta_energy / std::abs(initial.energy()) * 100.0 << " %\n";
    }
    output << "  Momento lineal: (" << initial.momentum.x << ", " << initial.momentum.y
           << ") -> (" << final.momentum.x << ", " << final.momentum.y << ")\n"
           << "  Norma del cambio de momento: " << delta_momentum << '\n'
           << "  Centro de masa: (" << initial.center_of_mass.x << ", " << initial.center_of_mass.y
           << ") -> (" << final.center_of_mass.x << ", " << final.center_of_mass.y << ")\n"
           << "  Error del centro de masa respecto al movimiento esperado: " << center_error << '\n';
}
}  // namespace nbody
