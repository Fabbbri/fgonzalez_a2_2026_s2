#include <iostream>
#include <chrono>
#include <cmath>
#include <vector>

#include "mnist.h"
#include "checksum.h"
#include "runtime.h"

using std::cout;

using Eigen::VectorXd;
using Eigen::MatrixXd;

#include "update_pool.h"

// ============================================================
// SIGMOIDE
// ============================================================

double sigmoid(double z)
{
    return 1.0 / (1.0 + std::exp(-z));
}

// ============================================================
// MAIN
// ============================================================

int run(int argc, char** argv) {
    dl::initialize();
    // --------------------------------------------------------
    // CONFIGURACIÓN
    // --------------------------------------------------------

    const double learning_rate = 0.1;
    const int num_muestras = dl::env_int("DL_SAMPLES", 100);
    const int epochs = (argc > 1) ? dl::integer(argv[1]) : 1000;

    const int workers = argc > 2 ? dl::integer(argv[2]) : 2;
    const int interval = argc > 3 ? dl::integer(argv[3], 0) : 10;
    const int duration = argc > 4 ? dl::integer(argv[4], 0) : 1;

    int conteo_clases[10] = {0};

    // --------------------------------------------------------
    // PESOS Y SESGOS
    // --------------------------------------------------------

    MatrixXd W1 = MatrixXd::Random(128, 784) * 0.1;
    VectorXd b1 = VectorXd::Zero(128);

    MatrixXd W2 = MatrixXd::Random(64, 128) * 0.1;
    VectorXd b2 = VectorXd::Zero(64);

    MatrixXd W3 = MatrixXd::Random(10, 64) * 0.1;
    VectorXd b3 = VectorXd::Zero(10);

    // --------------------------------------------------------
    // ARCHIVOS MNIST
    // --------------------------------------------------------

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
    // --------------------------------------------------------
    // ESTADO COMPARTIDO Y CREACIÓN DE HILOS
    // --------------------------------------------------------

    // Reservar el estado antes del cronómetro, como en main/CMP/SMT.
    VectorXd a1 = VectorXd::Zero(128);
    VectorXd a2 = VectorXd::Zero(64);
    VectorXd a3 = VectorXd::Zero(10);
    VectorXd dZ1 = VectorXd::Zero(128);
    VectorXd dZ2 = VectorXd::Zero(64);
    VectorXd dZ3 = VectorXd::Zero(10);
    MatrixXd dW1(128, 784);
    MatrixXd dW2(64, 128);
    MatrixXd dW3(10, 64);

    auto start = std::chrono::steady_clock::now();
    UpdatePool pool(workers, interval, duration);

    // --------------------------------------------------------
    // ENTRENAMIENTO SGD POR IMAGEN
    // --------------------------------------------------------

    for (int epoch = 0; epoch < epochs; epoch++) {

        double mse_total = 0.0;
        int aciertos = 0;

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

            // ---------- MSE y delta de salida, igual que main ----------
            mse_total += (a3 - sample.y).squaredNorm() / 10.0;
            for (int i = 0; i < 10; i++) {
                double dA3 = (2.0 / 10.0) * (a3(i) - sample.y(i));
                dZ3(i) = dA3 * a3(i) * (1.0 - a3(i));
            }

            // ---------- BACKPROP capa 2 y gradientes de W3 por columnas ----------
            for (int j = 0; j < 64; j++) {
                double dA2 = W3.col(j).dot(dZ3);
                dZ2(j) = dA2 * a2(j) * (1.0 - a2(j));
                for (int i = 0; i < 10; i++) {
                    dW3(i, j) = dZ3(i) * a2(j);
                }
            }

            // ---------- BACKPROP capa 1 y gradientes de W2 por columnas ----------
            for (int k = 0; k < 128; k++) {
                double dA1 = W2.col(k).dot(dZ2);
                dZ1(k) = dA1 * a1(k) * (1.0 - a1(k));
                for (int i = 0; i < 64; i++) {
                    dW2(i, k) = dZ2(i) * a1(k);
                }
            }

            // ---------- Gradientes de W1 por filas ----------
            for (int i = 0; i < 128; i++) {
                for (int j = 0; j < 784; j++) {
                    dW1(i, j) = dZ1(i) * x(j);
                }
            }

            // ---------- Actualizaciones concurrentes de todas las capas ----------
            // Publicar solo después de calcular todos los gradientes con los
            // pesos anteriores. La barrera del pool precede la siguiente imagen.
            pool.apply({
                {W1.data(), dW1.data(), static_cast<size_t>(W1.size())},
                {b1.data(), dZ1.data(), static_cast<size_t>(b1.size())},
                {W2.data(), dW2.data(), static_cast<size_t>(W2.size())},
                {b2.data(), dZ2.data(), static_cast<size_t>(b2.size())},
                {W3.data(), dW3.data(), static_cast<size_t>(W3.size())},
                {b3.data(), dZ3.data(), static_cast<size_t>(b3.size())}
            }, learning_rate);

            // ================================================
            // ACCURACY
            // ================================================

            Eigen::Index clase_real;
            Eigen::Index clase_predicha;

            sample.y.maxCoeff(&clase_real);
            a3.maxCoeff(&clase_predicha);

            if (epoch == 0) {
                conteo_clases[clase_real]++;
            }

            if (clase_real == clase_predicha) {
                aciertos++;
            }
        }

        double mse_promedio = mse_total / num_muestras;

        double accuracy =
            static_cast<double>(aciertos) / num_muestras * 100.0;

        cout << "Epoch: " << epoch
             << " | MSE: " << mse_promedio
             << " | Accuracy: " << accuracy << "%\n";
    }

    // --------------------------------------------------------
    // FINALIZAR TRABAJADORES
    // --------------------------------------------------------

    pool.close();

    // --------------------------------------------------------
    // RESULTADOS
    // --------------------------------------------------------

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

    if (!dl::quiet()) {
    cout << "\nDistribución de clases:\n";

    for (int i = 0; i < 10; i++) {
        cout << "Clase " << i << ": "
             << conteo_clases[i] << '\n';
    }

    cout << "\n=== Pesos y sesgos finales ===\n";

    cout << "W1:\n" << W1 << "\n";
    cout << "b1:\n" << b1 << "\n";

    cout << "W2:\n" << W2 << "\n";
    cout << "b2:\n" << b2 << "\n";

    cout << "W3:\n" << W3 << "\n";
    cout << "b3:\n" << b3 << "\n";

    }
    cout << "\n=== Coarse-Grained ===\n";
    cout << "Tiempo total: " << elapsed.count() << " us\n";
    cout << "Hilos: " << workers << "\n";
    cout << "Bloqueos periódicos: " << pool.blocks() << "\n";
    cout << "Hilo principal: entrenamiento y coordinación\n";

    return 0;
}
int main(int argc, char** argv) {
    try { return run(argc, argv); }
    catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << "\n";
        return 1;
    }
}
