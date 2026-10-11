#include "nbody.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace nbody {
struct Diagnostics {
    double mass = 0.0;
    double kinetic_energy = 0.0;
    double potential_energy = 0.0;
    Vec2 momentum;
    Vec2 center_of_mass;

    double energy() const { return kinetic_energy + potential_energy; }
};

// Diagnóstico independiente usado solo para validar el motor en las pruebas.
Diagnostics measure_physics(const std::vector<Body>& bodies) {
    Diagnostics result;
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const Body& body = bodies[i];
        if (!std::isfinite(body.mass) || body.mass <= 0.0) {
            throw std::runtime_error("El diagnóstico requiere masas positivas y finitas.");
        }
        result.mass += body.mass;
        result.kinetic_energy += 0.5 * body.mass *
            (body.velocity.x * body.velocity.x + body.velocity.y * body.velocity.y);
        result.momentum.x += body.mass * body.velocity.x;
        result.momentum.y += body.mass * body.velocity.y;
        result.center_of_mass.x += body.mass * body.position.x;
        result.center_of_mass.y += body.mass * body.position.y;

        // Cada pareja se cuenta una vez. El potencial usa el mismo suavizado
        // que las fuerzas: U_ij = -G*m_i*m_j / sqrt(r_ij² + epsilon²).
        for (std::size_t j = i + 1; j < bodies.size(); ++j) {
            const double dx = bodies[j].position.x - body.position.x;
            const double dy = bodies[j].position.y - body.position.y;
            result.potential_energy -= gravitational_constant * body.mass * bodies[j].mass /
                std::sqrt(dx * dx + dy * dy + softening * softening);
        }
    }
    if (result.mass == 0.0) {
        throw std::runtime_error("No hay cuerpos para calcular el centro de masa.");
    }
    result.center_of_mass.x /= result.mass;
    result.center_of_mass.y /= result.mass;
    if (!std::isfinite(result.mass) || !std::isfinite(result.energy()) ||
        !std::isfinite(result.momentum.x) || !std::isfinite(result.momentum.y) ||
        !std::isfinite(result.center_of_mass.x) || !std::isfinite(result.center_of_mass.y)) {
        throw std::runtime_error("Diagnóstico físico no finito.");
    }
    return result;
}
}  // namespace nbody




using namespace nbody;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool near(double actual, double expected, double tolerance = 1e-11) {
    return std::abs(actual - expected) < tolerance;
}

double orbit_drift(double dt, std::size_t steps) {
    auto model = make_sequential_model();
    const double speed = std::sqrt(0.25 / std::pow(1 + softening * softening, 1.5));
    std::vector<Body> bodies{{0.5, {-0.5, 0}, {0, -speed}, {}, 0},
                             {0.5, {0.5, 0}, {0, speed}, {}, 1}};
    std::vector<Vec2> accelerations;
    refresh_accelerations(bodies, accelerations, *model);
    const double initial_energy = measure_physics(bodies).energy();
    double max_drift = 0;
    for (std::size_t i = 0; i < steps; ++i) {
        advance_one_step(bodies, accelerations, dt, *model);
        const auto physics = measure_physics(bodies);
        max_drift = std::max(max_drift, std::abs((physics.energy() - initial_energy) / initial_energy));
        check(near(physics.momentum.x, 0) && near(physics.momentum.y, 0), "Momento orbital no conservado");
    }
    return max_drift;
}

void test_fine_grained() {
    auto reference = make_sequential_model();
    for (std::size_t quantum : {std::size_t{1}, std::size_t{2}, std::size_t{3},
                                std::size_t{100}, std::numeric_limits<std::size_t>::max()}) {
        auto fine = make_fine_grained_model(quantum);
        for (std::size_t count : {0, 1, 2, 3, 7, 12}) {
            auto bodies = create_galaxies(6, 19);
            bodies.resize(count);
            std::vector<Vec2> expected, actual;
            reference->compute_accelerations(bodies, expected, 0.7, 0.03);
            const auto before = fine->scheduling_stats();
            fine->compute_accelerations(bodies, actual, 0.7, 0.03);
            check(actual.size() == expected.size(), "Fine-Grained pierde cuerpos");
            for (std::size_t i = 0; i < count; ++i) {
                check(actual[i].x == expected[i].x && actual[i].y == expected[i].y,
                      "Fine-Grained altera el orden de suma o pierde interacciones");
            }
            const std::size_t rounds = count == 0 ? 0 : count == 1 ? 1 : 1 + (count - 2) / quantum;
            const auto after = fine->scheduling_stats();
            check(after.tasks == count && after.rounds - before.rounds == rounds &&
                  after.dispatches - before.dispatches == count * rounds,
                  "Quantum o reinicio de contextos incorrecto");
        }
    }
    bool rejected = false;
    try { make_fine_grained_model(0); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "Admite quantum cero");
}

void test_coarse_grained() {
    auto reference = make_sequential_model();
    for (std::size_t workers : {1, 2, 3, 12}) {
        auto coarse = make_coarse_grained_model(workers);
        check(coarse->workers() == workers, "Cantidad de hilos incorrecta");
        // Reutilizar el pool con tamaños cambiantes, bloques vacíos y residuos.
        for (std::size_t count : {0, 1, 2, 7, 12, 3, 0}) {
            auto bodies = create_galaxies(6, 19);
            bodies.resize(count);
            for (int phase = 0; phase < 20; ++phase) {
                std::vector<Vec2> expected, actual;
                const double gravity = 0.7 + phase * 0.01;
                reference->compute_accelerations(bodies, expected, gravity, 0.03);
                const auto before = coarse->scheduling_stats();
                coarse->compute_accelerations(bodies, actual, gravity, 0.03);
                check(actual.size() == expected.size(), "Coarse-Grained pierde cuerpos");
                for (std::size_t i = 0; i < count; ++i) {
                    check(actual[i].x == expected[i].x && actual[i].y == expected[i].y,
                          "Coarse-Grained altera fuerzas o retorna antes de terminar");
                    bodies[i].position.x += 0.001;
                }
                const auto after = coarse->scheduling_stats();
                check(after.rounds == before.rounds + 1 &&
                      after.dispatches == before.dispatches + workers,
                      "Conteo de fases Coarse-Grained incorrecto");
            }
        }
    }
    // Destruir un pool que todavía no recibió trabajo también debe finalizar.
    make_coarse_grained_model(2);
    bool rejected = false;
    try { make_coarse_grained_model(0); } catch (const std::invalid_argument&) { rejected = true; }
    check(rejected, "Admite cero hilos");
}

int main() try {
    test_fine_grained();
    test_coarse_grained();
    auto model = make_sequential_model();
    // Dos masas con energía, momento y centro de masa calculables a mano.
    std::vector<Body> bodies{{2, {0, 0}, {1, -2}, {}, 0}, {3, {3, 4}, {0, 1}, {}, 1}};
    const auto initial = measure_physics(bodies);
    check(near(initial.mass, 5), "Masa total incorrecta");
    check(near(initial.kinetic_energy, 6.5), "Energía cinética incorrecta");
    check(near(initial.potential_energy, -6 / std::sqrt(25 + softening * softening)), "Potencial incorrecto");
    check(near(initial.momentum.x, 2) && near(initial.momentum.y, -1), "Momento incorrecto");
    check(near(initial.center_of_mass.x, 1.8) && near(initial.center_of_mass.y, 2.4), "Centro de masa incorrecto");

    std::vector<Vec2> accelerations;
    refresh_accelerations(bodies, accelerations, *model);
    const auto before = bodies;
    advance_one_step(bodies, accelerations, 0.01, *model);
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const double vx = before[i].velocity.x + before[i].acceleration.x * 0.01;
        const double vy = before[i].velocity.y + before[i].acceleration.y * 0.01;
        check(near(bodies[i].velocity.x, vx) && near(bodies[i].velocity.y, vy), "Velocidad incorrecta");
        check(near(bodies[i].position.x, before[i].position.x + vx * 0.01) &&
              near(bodies[i].position.y, before[i].position.y + vy * 0.01), "No se usa Euler simpléctico");
    }
    for (std::size_t i = 1; i < 100; ++i) advance_one_step(bodies, accelerations, 0.01, *model);
    const auto final = measure_physics(bodies);
    check(near(final.momentum.x, initial.momentum.x) && near(final.momentum.y, initial.momentum.y), "Cambio de momento");
    check(near(final.center_of_mass.x, initial.center_of_mass.x + initial.momentum.x / initial.mass) &&
          near(final.center_of_mass.y, initial.center_of_mass.y + initial.momentum.y / initial.mass), "Movimiento del centro de masa incorrecto");

    bodies = {{1, {0, 0}, {}, {}, 0}, {1, {0, 0}, {}, {}, 1}};
    refresh_accelerations(bodies, accelerations, *model);
    check(near(bodies[0].acceleration.x, 0) && near(bodies[0].acceleration.y, 0), "Singularidad en cuerpos coincidentes");
    check(near(measure_physics(bodies).potential_energy, -1 / softening), "Potencial coincidente incorrecto");
    bodies.resize(1);
    check(near(measure_physics(bodies).energy(), 0), "Energía incorrecta para un cuerpo inmóvil aislado");

    const double coarse = orbit_drift(0.01, 1000);
    const double fine = orbit_drift(0.005, 2000);
    check(coarse < 0.001 && fine < coarse, "La órbita no converge al reducir dt");
    std::cout << "Pruebas físicas correctas.\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
