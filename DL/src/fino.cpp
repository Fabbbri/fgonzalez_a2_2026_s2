#include <iostream>
#include <chrono>
#include <cmath>
#include <vector>
#include <fstream>
#include <cstdlib>
#include "mnist.h"
#include "checksum.h"
#include "runtime.h"

using std::cout;
using Eigen::VectorXd;
using Eigen::MatrixXd;

auto QUANTUM = std::chrono::microseconds(100);
const int NUM_HILOS = 4;

// =========================================
// MODELO
// =========================================

struct ModeloFG {
    MatrixXd W1, W2, W3;
    VectorXd b1, b2, b3;
};

struct Estado {
    VectorXd x, y;
    VectorXd z1, a1, z2, a2, z3, a3;
    VectorXd dA3, dZ3, dA2, dZ2, dA1, dZ1;

    MatrixXd gradW1, gradW2, gradW3;
    VectorXd gradb1, gradb2, gradb3;
};

// =========================================
// PLANIFICADOR FINE-GRAINED
// Sin vectores de funciones ni asignaciones
// dinámicas para cada operación.
// =========================================

template <typename Funcion>
void ejecutar_etapa(int cantidad, Funcion operacion) {
    int siguiente[NUM_HILOS] = {0, 1, 2, 3};
    bool terminados[NUM_HILOS] = {false, false, false, false};

    int actual = 0;
    int restantes = cantidad;

    while (restantes > 0) {
        if (terminados[actual]) {
            actual = (actual + 1) % NUM_HILOS;
            continue;
        }

        auto inicio = std::chrono::steady_clock::now();

        while (std::chrono::steady_clock::now() - inicio < QUANTUM) {
            int indice = siguiente[actual];

            if (indice >= cantidad) {
                terminados[actual] = true;
                break;
            }

            operacion(indice);
            siguiente[actual] += NUM_HILOS;
            restantes -= 1;
        }

        actual = (actual + 1) % NUM_HILOS;
    }
}

double sigmoid(double x) {
    return 1.0 / (1.0 + std::exp(-x));
}

// =========================================
// ENTRENAMIENTO DE UNA IMAGEN: SGD
// =========================================

void entrenar_muestra(
    ModeloFG& modelo,
    const Sample& muestra,
    double learning_rate,
    double& mse,
    int& aciertos
) {
    Estado e;

    e.x = muestra.x;
    e.y = muestra.y;

    e.z1 = VectorXd::Zero(128);
    e.a1 = VectorXd::Zero(128);
    e.z2 = VectorXd::Zero(64);
    e.a2 = VectorXd::Zero(64);
    e.z3 = VectorXd::Zero(10);
    e.a3 = VectorXd::Zero(10);

    e.dA3 = VectorXd::Zero(10);
    e.dZ3 = VectorXd::Zero(10);
    e.dA2 = VectorXd::Zero(64);
    e.dZ2 = VectorXd::Zero(64);
    e.dA1 = VectorXd::Zero(128);
    e.dZ1 = VectorXd::Zero(128);

    e.gradW1 = MatrixXd::Zero(128, 784);
    e.gradb1 = VectorXd::Zero(128);
    e.gradW2 = MatrixXd::Zero(64, 128);
    e.gradb2 = VectorXd::Zero(64);
    e.gradW3 = MatrixXd::Zero(10, 64);
    e.gradb3 = VectorXd::Zero(10);

    // -----------------------------------------
    // FORWARD: Z1 = W1 * X + b1
    // -----------------------------------------

    ejecutar_etapa(128, [&](int i) {
        e.z1(i) = modelo.W1.row(i).dot(e.x) + modelo.b1(i);
    });

    ejecutar_etapa(128, [&](int i) {
        e.a1(i) = sigmoid(e.z1(i));
    });

    // -----------------------------------------
    // FORWARD: Z2 = W2 * A1 + b2
    // -----------------------------------------

    ejecutar_etapa(64, [&](int i) {
        e.z2(i) = modelo.W2.row(i).dot(e.a1) + modelo.b2(i);
    });

    ejecutar_etapa(64, [&](int i) {
        e.a2(i) = sigmoid(e.z2(i));
    });

    // -----------------------------------------
    // FORWARD: Z3 = W3 * A2 + b3
    // -----------------------------------------

    ejecutar_etapa(10, [&](int i) {
        e.z3(i) = modelo.W3.row(i).dot(e.a2) + modelo.b3(i);
    });

    ejecutar_etapa(10, [&](int i) {
        e.a3(i) = sigmoid(e.z3(i));
    });

    // Métricas de esta imagen
    mse += (e.a3 - e.y).squaredNorm() / 10.0;

    Eigen::Index clase_real, clase_predicha;
    e.y.maxCoeff(&clase_real);
    e.a3.maxCoeff(&clase_predicha);

    if (clase_real == clase_predicha) {
        aciertos++;
    }

    // =========================================
    // BACKPROPAGATION
    // =========================================

    // Capa de salida: derivada del MSE
    ejecutar_etapa(10, [&](int i) {
        e.dA3(i) = (2.0 / 10.0) * (e.a3(i) - e.y(i));
    });

    // Derivada de sigmoid y gradiente del sesgo
    ejecutar_etapa(10, [&](int i) {
        e.dZ3(i) = e.dA3(i) * e.a3(i) * (1.0 - e.a3(i));
        e.gradb3(i) = e.dZ3(i);
    });

    // Gradiente de W3
    ejecutar_etapa(10 * 64, [&](int k) {
        int i = k / 64;
        int j = k % 64;
        e.gradW3(i, j) = e.dZ3(i) * e.a2(j);
    });

    // Propagar a la capa oculta anterior
    ejecutar_etapa(64, [&](int j) {
        e.dA2(j) = modelo.W3.col(j).dot(e.dZ3);
    });

    ejecutar_etapa(64, [&](int i) {
        e.dZ2(i) = e.dA2(i) * e.a2(i) * (1.0 - e.a2(i));
        e.gradb2(i) = e.dZ2(i);
    });

    ejecutar_etapa(64 * 128, [&](int k) {
        int i = k / 128;
        int j = k % 128;
        e.gradW2(i, j) = e.dZ2(i) * e.a1(j);
    });

    ejecutar_etapa(128, [&](int j) {
        e.dA1(j) = modelo.W2.col(j).dot(e.dZ2);
    });

    ejecutar_etapa(128, [&](int i) {
        e.dZ1(i) = e.dA1(i) * e.a1(i) * (1.0 - e.a1(i));
        e.gradb1(i) = e.dZ1(i);
    });

    ejecutar_etapa(128 * 784, [&](int k) {
        int i = k / 784;
        int j = k % 784;
        e.gradW1(i, j) = e.dZ1(i) * e.x(j);
    });

    // =========================================
    // SGD: ACTUALIZAR PESOS PARA ESTA IMAGEN
    // Se usan los gradientes calculados antes
    // de modificar cualquier parámetro.
    // =========================================

    ejecutar_etapa(10 * 64, [&](int k) {
        int i = k / 64;
        int j = k % 64;
        modelo.W3(i, j) -= learning_rate * e.gradW3(i, j);
    });

    ejecutar_etapa(10, [&](int i) {
        modelo.b3(i) -= learning_rate * e.gradb3(i);
    });

    ejecutar_etapa(64 * 128, [&](int k) {
        int i = k / 128;
        int j = k % 128;
        modelo.W2(i, j) -= learning_rate * e.gradW2(i, j);
    });

    ejecutar_etapa(64, [&](int i) {
        modelo.b2(i) -= learning_rate * e.gradb2(i);
    });

    ejecutar_etapa(128 * 784, [&](int k) {
        int i = k / 784;
        int j = k % 784;
        modelo.W1(i, j) -= learning_rate * e.gradW1(i, j);
    });

    ejecutar_etapa(128, [&](int i) {
        modelo.b1(i) -= learning_rate * e.gradb1(i);
    });
}

// =========================================
// MAIN
// =========================================

int run(int argc, char** argv) {
    dl::initialize();
    QUANTUM = std::chrono::microseconds(dl::env_int("DL_QUANTUM_US", 100));
    const double learning_rate = 0.1;
    const int num_muestras = dl::env_int("DL_SAMPLES", 100);
    const int epochs = (argc > 1) ? dl::integer(argv[1]) : 1000;

    ModeloFG modelo;

    modelo.W1 = MatrixXd::Random(128, 784) * 0.1;
    modelo.b1 = VectorXd::Zero(128);

    modelo.W2 = MatrixXd::Random(64, 128) * 0.1;
    modelo.b2 = VectorXd::Zero(64);

    modelo.W3 = MatrixXd::Random(10, 64) * 0.1;
    modelo.b3 = VectorXd::Zero(10);

    std::ifstream image_file(
        "../data/mnist/train-images-idx3-ubyte",
        std::ios::binary
    );

    std::ifstream label_file(
        "../data/mnist/train-labels-idx1-ubyte",
        std::ios::binary
    );

    if (!image_file || !label_file) {
        std::cerr << "No se pudieron abrir los archivos MNIST\n";
        return 1;
    }

    std::vector<Sample> muestras;
    for (int i = 0; i < num_muestras; i++) {
        muestras.push_back(load_sample(image_file, label_file, i));
    }

    auto start = std::chrono::steady_clock::now();

    for (int epoch = 0; epoch < epochs; epoch++) {
        double mse = 0.0;
        int aciertos = 0;

        // Mantener el orden secuencial de las muestras.
        for (int i = 0; i < num_muestras; i++) {
            entrenar_muestra(
                modelo, muestras[i], learning_rate, mse, aciertos
            );
        }

        cout << "Epoch: " << epoch
             << " | MSE: " << mse / num_muestras
             << " | Accuracy: "
             << 100.0 * aciertos / num_muestras << "%\n";
    }

    auto end = std::chrono::steady_clock::now();

    auto elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(
            end - start
        );

    dl::finish(modelo.W1, modelo.b1, modelo.W2, modelo.b2, modelo.W3, modelo.b3);
    uint64_t h = CHECKSUM_INICIAL;
    h = hash_params(modelo.W1, h); h = hash_params(modelo.b1, h);
    h = hash_params(modelo.W2, h); h = hash_params(modelo.b2, h);
    h = hash_params(modelo.W3, h); h = hash_params(modelo.b3, h);
    cout << "\nChecksum parametros: " << std::hex << h << std::dec << "\n";

    if (!dl::quiet()) {
    cout << "\n=== Pesos y sesgos finales ===\n";

    cout << "W1:\n" << modelo.W1 << "\n";
    cout << "b1:\n" << modelo.b1 << "\n";

    cout << "W2:\n" << modelo.W2 << "\n";
    cout << "b2:\n" << modelo.b2 << "\n";

    cout << "W3:\n" << modelo.W3 << "\n";
    cout << "b3:\n" << modelo.b3 << "\n";

    }
    cout << "\n=== Fine-Grained ===\n";
    cout << "Tiempo total: " << elapsed.count() << " us\n";

    return 0;
}
int main(int argc, char** argv) {
    try { return run(argc, argv); }
    catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        return 1;
    }
}
