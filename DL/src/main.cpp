#include <iostream>
#include <chrono>
#include <cstdlib>
#include <cmath>
#include "checksum.h"
#include "runtime.h"
#include "mnist.h"
#include <vector>

using std::cout;

using Eigen::VectorXd;
using Eigen::MatrixXd;

double sigmoid(double z) {
    return 1.0 / (1.0 + std::exp(-z));
}

int run(int argc, char** argv) {
    dl::initialize();

    // =========================================
    // CONFIGURACIÓN DEL ENTRENAMIENTO
    // =========================================

    // n (\eta indica que tanto avanza en la direccion del gradiente)
    double learning_rate = 0.1;
    const int num_muestras = dl::env_int("DL_SAMPLES", 100);
    const int epochs = (argc > 1) ? dl::integer(argv[1]) : 1000;
    double mse_promedio = 0.0;
    int conteo_clases[10] = {0};

    // =========================================
    // PESOS Y SESGOS
    // =========================================

    MatrixXd W1 = MatrixXd::Random(128, 784) * 0.1;
    VectorXd b1 = VectorXd::Zero(128);

    MatrixXd W2 = MatrixXd::Random(64, 128) * 0.1;
    VectorXd b2 = VectorXd::Zero(64);

    MatrixXd W3 = MatrixXd::Random(10, 64) * 0.1;
    VectorXd b3 = VectorXd::Zero(10);

    // Igual que CMP/SMT: reservar el estado antes de medir el entrenamiento.
    VectorXd a1 = VectorXd::Zero(128);
    VectorXd a2 = VectorXd::Zero(64);
    VectorXd a3 = VectorXd::Zero(10);
    VectorXd dZ1 = VectorXd::Zero(128);
    VectorXd dZ2 = VectorXd::Zero(64);
    VectorXd dZ3 = VectorXd::Zero(10);

    // =========================================
    // LECTOR.CPP PARA RECIBIR ENTRADA
    // =========================================

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
    for (int i = 0; i < num_muestras; i++){
        muestras.push_back(load_sample(image_file, label_file, i));
    }

    // Justo al iniciar el entrenamiento
    auto start = std::chrono::steady_clock::now();

    // =========================================
    // ENTRENAMIENTO SGD POR IMAGEN
    // Mismo kernel por filas/columnas que CMP/SMT, en un hilo y sin barreras.
    // =========================================

    for (int epoch = 0; epoch < epochs; epoch++) {
        double mse_total = 0.0;
        int aciertos = 0;
        Eigen::Index clase_real;
        Eigen::Index clase_predicha;

        for (int s = 0; s < num_muestras; s++) {
            const Sample& sample = muestras[s];

            const VectorXd& x = sample.x;

            // ---------- FORWARD capa 1 (filas de W1: 784 -> 128) ----------
            for (int i = 0; i < 128; i++) {
                double z = W1.row(i).dot(x) + b1(i);
                a1(i) = sigmoid(z);
            }

            // ---------- FORWARD capa 2 (filas de W2: 128 -> 64) ----------
            for (int i = 0; i < 64; i++) {
                double z = W2.row(i).dot(a1) + b2(i);
                a2(i) = sigmoid(z);
            }

            // ---------- FORWARD capa 3 (filas de W3: 64 -> 10) ----------
            for (int i = 0; i < 10; i++) {
                double z = W3.row(i).dot(a2) + b3(i);
                a3(i) = sigmoid(z);
            }

            // ---------- MSE y delta de salida, igual que CMP/SMT ----------
            mse_total += (a3 - sample.y).squaredNorm() / 10.0;
            for (int i = 0; i < 10; i++) {
                double dA3 = (2.0 / 10.0) * (a3(i) - sample.y(i));
                dZ3(i) = dA3 * a3(i) * (1.0 - a3(i));
                b3(i) -= learning_rate * dZ3(i);
            }

            // ---------- BACKPROP capa 2 y actualizar columnas de W3 ----------
            // Leer cada columna anterior antes de actualizarla conserva SGD.
            for (int j = 0; j < 64; j++) {
                double dA2 = W3.col(j).dot(dZ3);
                dZ2(j) = dA2 * a2(j) * (1.0 - a2(j));
                for (int i = 0; i < 10; i++) {
                    W3(i, j) -= learning_rate * (dZ3(i) * a2(j));
                }
                b2(j) -= learning_rate * dZ2(j);
            }

            // ---------- BACKPROP capa 1 y actualizar columnas de W2 ----------
            for (int k = 0; k < 128; k++) {
                double dA1 = W2.col(k).dot(dZ2);
                dZ1(k) = dA1 * a1(k) * (1.0 - a1(k));
                for (int i = 0; i < 64; i++) {
                    W2(i, k) -= learning_rate * (dZ2(i) * a1(k));
                }
                b1(k) -= learning_rate * dZ1(k);
            }

            // ---------- ACTUALIZAR W1 por filas ----------
            for (int i = 0; i < 128; i++) {
                for (int j = 0; j < 784; j++) {
                    W1(i, j) -= learning_rate * (dZ1(i) * x(j));
                }
            }

            // ---------- Accuracy y distribución de clases ----------
            if (epoch == 0) {
                Eigen::Index clase_real;
                sample.y.maxCoeff(&clase_real);
                conteo_clases[clase_real]++;
            }

            a3.maxCoeff(&clase_predicha);
            sample.y.maxCoeff(&clase_real);
            if (clase_predicha == clase_real) {
                aciertos++;
            }
        }

        // ---------- Métricas de la época ----------
        mse_promedio = mse_total / num_muestras;
        double accuracy = (double) aciertos / num_muestras * 100.0;
        cout << "MSE promedio: " << mse_promedio
             << " | Accuracy: " << accuracy << "%\n";
    }
    // =========================================
    // RESULTADOS FINALES
    // =========================================

    auto end = std::chrono::steady_clock::now();

    auto elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(
            end - start
        );

    dl::finish(W1, b1, W2, b2, W3, b3);
    uint64_t h = CHECKSUM_INICIAL;
    h = hash_params(W1, h); h = hash_params(b1, h);
    h = hash_params(W2, h); h = hash_params(b2, h);
    h = hash_params(W3, h); h = hash_params(b3, h);
    cout << "\nChecksum parametros: " << std::hex << h << std::dec << "\n";

    // Cuantas imágenes corresponden a cada número
    if (!dl::quiet()) {
    cout << "\nDistribución de clases:\n";

    for (int i = 0; i < 10; i++) {
        cout << "Clase " << i << ": "
             << conteo_clases[i] << "\n";
    }

    cout << "\n=== Pesos y sesgos finales ===\n";

    cout << "W1:\n" << W1 << "\n";
    cout << "b1:\n" << b1 << "\n";

    cout << "W2:\n" << W2 << "\n";
    cout << "b2:\n" << b2 << "\n";

    cout << "W3:\n" << W3 << "\n";
    cout << "b3:\n" << b3 << "\n";
    
    }
    cout << "\n=== Secuencial ===\n";

    cout << "\nTiempo total: "
         << elapsed.count()
         << " us\n";

    return 0;
}
int main(int argc, char** argv) {
    try { return run(argc, argv); }
    catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        return 1;
    }
}
