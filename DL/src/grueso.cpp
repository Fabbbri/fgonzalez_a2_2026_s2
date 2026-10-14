#include <iostream>
#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>

#include "mnist.h"
#include "checksum.h"

using std::cout;

using Eigen::VectorXd;
using Eigen::MatrixXd;

// ============================================================
// DATOS COMPARTIDOS PARA LAS ACTUALIZACIONES
// ============================================================

struct SharedUpdate {
    MatrixXd* W2;
    VectorXd* b2;

    MatrixXd* W3;
    VectorXd* b3;

    const MatrixXd* dW2;
    const VectorXd* db2;

    const MatrixXd* dW3;
    const VectorXd* db3;

    double learning_rate;

    std::mutex mutex;
    std::condition_variable cv_job;
    std::condition_variable cv_done;

    int generation = 0;
    int completed = 0;
    bool stop = false;
};

// ============================================================
// TRABAJADORES PERSISTENTES
// ============================================================

void update_worker(int id, SharedUpdate& shared)
{
    int last_generation = 0;

    while (true) {
        std::unique_lock<std::mutex> lock(shared.mutex);

        shared.cv_job.wait(lock, [&] {
            return shared.stop ||
                   shared.generation != last_generation;
        });

        if (shared.stop) {
            return;
        }

        last_generation = shared.generation;

        lock.unlock();

        // Cada trabajador actualiza parámetros diferentes.
        if (id == 0) {
            *shared.W3 -= shared.learning_rate * (*shared.dW3);
            *shared.b3 -= shared.learning_rate * (*shared.db3);
        } else {
            *shared.W2 -= shared.learning_rate * (*shared.dW2);
            *shared.b2 -= shared.learning_rate * (*shared.db2);
        }

        lock.lock();

        shared.completed++;

        if (shared.completed == 2) {
            shared.cv_done.notify_one();
        }
    }
}

// ============================================================
// SIGMOIDE
// ============================================================

VectorXd sigmoid(const VectorXd& z)
{
    return 1.0 / (1.0 + (-z.array()).exp());
}

// ============================================================
// MAIN
// ============================================================

int main(int argc, char** argv) {
    // --------------------------------------------------------
    // CONFIGURACIÓN
    // --------------------------------------------------------

    const double learning_rate = 0.1;
    const int num_muestras = 100;
    const int epochs = (argc > 1) ? std::atoi(argv[1]) : 1000;

    int conteo_clases[10] = {0};

    // --------------------------------------------------------
    // PESOS Y SESGOS
    // --------------------------------------------------------

    MatrixXd W1 = MatrixXd::Random(128, 784) * 0.1;
    MatrixXd b1 = MatrixXd::Zero(128, 1);

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

    MatrixXd dW2(64, 128);
    VectorXd db2(64);

    MatrixXd dW3(10, 64);
    VectorXd db3(10);

    SharedUpdate shared{
        &W2, &b2,
        &W3, &b3,
        &dW2, &db2,
        &dW3, &db3,
        learning_rate,
        {},
        {},
        {},
        0,
        0,
        false
    };

    auto start = std::chrono::steady_clock::now();      // justo antes de crear los hilos
    std::thread worker1(update_worker, 0, std::ref(shared));
    std::thread worker2(update_worker, 1, std::ref(shared));

    // --------------------------------------------------------
    // ENTRENAMIENTO SGD POR IMAGEN
    // --------------------------------------------------------

    for (int epoch = 0; epoch < epochs; epoch++) {

        double mse_total = 0.0;
        int aciertos = 0;

        for (int i = 0; i < num_muestras; i++) {

            const Sample& sample = muestras[i];

            // ================================================
            // FORWARD PASS
            // ================================================

            VectorXd z1 = W1 * sample.x + b1;
            VectorXd a1 = sigmoid(z1);

            VectorXd z2 = W2 * a1 + b2;
            VectorXd a2 = sigmoid(z2);

            VectorXd z3 = W3 * a2 + b3;
            VectorXd a3 = sigmoid(z3);

            // ================================================
            // MSE
            // ================================================

            double mse =
                (a3 - sample.y).squaredNorm() / a3.size();

            mse_total += mse;

            // ================================================
            // BACKPROPAGATION: CAPA DE SALIDA
            // ================================================

            VectorXd dA3 =
                (2.0 / a3.size()) * (a3 - sample.y);

            VectorXd dZ3 =
                dA3.array()
                * a3.array()
                * (1.0 - a3.array());

            dW3 = dZ3 * a2.transpose();
            db3 = dZ3;

            // ================================================
            // BACKPROPAGATION: CAPA OCULTA 2
            // ================================================

            VectorXd dA2 = W3.transpose() * dZ3;

            VectorXd dZ2 =
                dA2.array()
                * a2.array()
                * (1.0 - a2.array());

            dW2 = dZ2 * a1.transpose();
            db2 = dZ2;

            // ================================================
            // BACKPROPAGATION: CAPA OCULTA 1
            // ================================================

            VectorXd dA1 = W2.transpose() * dZ2;

            VectorXd dZ1 =
                dA1.array()
                * a1.array()
                * (1.0 - a1.array());

            MatrixXd dW1 = dZ1 * sample.x.transpose();
            VectorXd db1 = dZ1;

            // ================================================
            // ACTUALIZACIONES CONCURRENTES
            // ================================================

            {
                std::lock_guard<std::mutex> lock(shared.mutex);

                shared.completed = 0;
                shared.generation++;
            }

            shared.cv_job.notify_all();

            // El hilo principal actualiza la primera capa.
            W1 -= learning_rate * dW1;
            b1 -= learning_rate * db1;

            // Esperar las actualizaciones de W2 y W3.
            {
                std::unique_lock<std::mutex> lock(shared.mutex);

                shared.cv_done.wait(lock, [&] {
                    return shared.completed == 2;
                });
            }

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

    {
        std::lock_guard<std::mutex> lock(shared.mutex);
        shared.stop = true;
    }

    shared.cv_job.notify_all();

    worker1.join();
    worker2.join();

    // --------------------------------------------------------
    // RESULTADOS
    // --------------------------------------------------------

    auto end = std::chrono::steady_clock::now();

    auto elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(
            end - start
        );

    uint64_t h = CHECKSUM_INICIAL;
    h = hash_params(W1, h); h = hash_params(b1, h);
    h = hash_params(W2, h); h = hash_params(b2, h);
    h = hash_params(W3, h); h = hash_params(b3, h);
    cout << "\nChecksum parametros: " << std::hex << h << std::dec << "\n";

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

    cout << "\n=== Coarse-Grained ===\n";
    cout << "Tiempo total: " << elapsed.count() << " us\n";
    cout << "Hilos trabajadores: 2\n";
    cout << "Hilo principal: entrenamiento y coordinación\n";

    return 0;
}