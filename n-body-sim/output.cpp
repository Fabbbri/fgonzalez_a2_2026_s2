#include "output.hpp"
#include <cmath>
#include <iomanip>
#include <ostream>

namespace nbody {
void write_physics_report(const Diagnostics& initial, const Diagnostics& final,
                          double time, std::ostream& output) {
    const double delta_energy = final.energy() - initial.energy();
    const double delta_momentum = std::hypot(final.momentum.x - initial.momentum.x,
                                              final.momentum.y - initial.momentum.y);
    // El centro de masa puede desplazarse: esperamos R(t) = R(0) + P(0)/M*t.
    const Vec2 expected_center{
        initial.center_of_mass.x + initial.momentum.x / initial.mass * time,
        initial.center_of_mass.y + initial.momentum.y / initial.mass * time};
    const double center_error = std::hypot(final.center_of_mass.x - expected_center.x,
                                            final.center_of_mass.y - expected_center.y);

    output << std::setprecision(17)
           << "Diagnóstico físico (inicial -> final; fuera del cronómetro):\n"
           << "  Masa: " << initial.mass << " -> " << final.mass << '\n'
           << "  Energía cinética: " << initial.kinetic_energy << " -> " << final.kinetic_energy << '\n'
           << "  Energía potencial: " << initial.potential_energy << " -> " << final.potential_energy << '\n'
           << "  Energía total: " << initial.energy() << " -> " << final.energy() << '\n'
           << "  Cambio de energía: " << delta_energy << '\n'
           << "  Cambio relativo de energía respecto a |E inicial|: ";
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
