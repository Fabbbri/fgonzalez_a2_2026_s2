#include "nbody/diagnostics.hpp"
#include "nbody/output.hpp"
#include "nbody/simulation.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace nbody;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool near(double actual, double expected, double tolerance = 1e-11) {
    return std::abs(actual - expected) < tolerance;
}

double orbit_drift(double dt, std::size_t steps) {
    const double speed = std::sqrt(0.25 / std::pow(1 + softening * softening, 1.5));
    std::vector<Body> bodies{{0.5, {-0.5, 0}, {0, -speed}, {}, 0},
                             {0.5, {0.5, 0}, {0, speed}, {}, 1}};
    std::vector<Vec2> accelerations;
    refresh_accelerations(bodies, accelerations);
    const double initial_energy = measure_physics(bodies).energy();
    double max_drift = 0;
    for (std::size_t i = 0; i < steps; ++i) {
        advance_one_step(bodies, accelerations, dt);
        const auto physics = measure_physics(bodies);
        max_drift = std::max(max_drift, std::abs((physics.energy() - initial_energy) / initial_energy));
        check(near(physics.momentum.x, 0) && near(physics.momentum.y, 0), "Momento orbital no conservado");
    }
    return max_drift;
}

int main() try {
    // Dos masas con energía, momento y centro de masa calculables a mano.
    std::vector<Body> bodies{{2, {0, 0}, {1, -2}, {}, 0}, {3, {3, 4}, {0, 1}, {}, 1}};
    const auto initial = measure_physics(bodies);
    check(near(initial.mass, 5), "Masa total incorrecta");
    check(near(initial.kinetic_energy, 6.5), "Energía cinética incorrecta");
    check(near(initial.potential_energy, -6 / std::sqrt(25 + softening * softening)), "Potencial incorrecto");
    check(near(initial.momentum.x, 2) && near(initial.momentum.y, -1), "Momento incorrecto");
    check(near(initial.center_of_mass.x, 1.8) && near(initial.center_of_mass.y, 2.4), "Centro de masa incorrecto");

    std::vector<Vec2> accelerations;
    refresh_accelerations(bodies, accelerations);
    const auto before = bodies;
    advance_one_step(bodies, accelerations, 0.01);
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const double vx = before[i].velocity.x + before[i].acceleration.x * 0.01;
        const double vy = before[i].velocity.y + before[i].acceleration.y * 0.01;
        check(near(bodies[i].velocity.x, vx) && near(bodies[i].velocity.y, vy), "Velocidad incorrecta");
        check(near(bodies[i].position.x, before[i].position.x + vx * 0.01) &&
              near(bodies[i].position.y, before[i].position.y + vy * 0.01), "No se usa Euler simpléctico");
    }
    for (std::size_t i = 1; i < 100; ++i) advance_one_step(bodies, accelerations, 0.01);
    const auto final = measure_physics(bodies);
    check(near(final.momentum.x, initial.momentum.x) && near(final.momentum.y, initial.momentum.y), "Cambio de momento");
    check(near(final.center_of_mass.x, initial.center_of_mass.x + initial.momentum.x / initial.mass) &&
          near(final.center_of_mass.y, initial.center_of_mass.y + initial.momentum.y / initial.mass), "Movimiento del centro de masa incorrecto");

    bodies = {{1, {0, 0}, {}, {}, 0}, {1, {0, 0}, {}, {}, 1}};
    refresh_accelerations(bodies, accelerations);
    check(near(bodies[0].acceleration.x, 0) && near(bodies[0].acceleration.y, 0), "Singularidad en cuerpos coincidentes");
    check(near(measure_physics(bodies).potential_energy, -1 / softening), "Potencial coincidente incorrecto");
    bodies.resize(1);
    const auto zero = measure_physics(bodies);
    std::ostringstream report;
    write_physics_report(zero, zero, 0, report);
    check(report.str().find("no definido") != std::string::npos, "No se trata la energía inicial cero");

    const double coarse = orbit_drift(0.01, 1000);
    const double fine = orbit_drift(0.005, 2000);
    check(coarse < 0.001 && fine < coarse, "La órbita no converge al reducir dt");
    std::cout << "Pruebas físicas correctas.\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
