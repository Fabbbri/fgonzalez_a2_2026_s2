#include <Eigen/Dense>

#include <pthread.h>
#include <sched.h>

#include <condition_variable>
#include <cmath>
#include <iostream>
#include <mutex>
#include <thread>
#include <chrono>

using std::cout;

// ============================================================
// Afinidad de CPU
// ============================================================

void pin_to_cpu(int cpu)
{
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);

    int result = pthread_setaffinity_np(
        pthread_self(),
        sizeof(cpu_set_t),
        &cpuset
    );

    if (result != 0)
    {
        std::cerr << "Error al fijar afinidad a CPU "
                  << cpu << "\n";
    }
}

// ============================================================
// Sigmoid
// ============================================================

double sigmoid(double x)
{
    return 1.0 / (1.0 + std::exp(-x));
}

// ============================================================
// Worker de cada neurona
// ============================================================

void compute_neuron(
    int neuron,
    int cpu,
    const Eigen::MatrixXd& W1,
    const Eigen::VectorXd& x,
    const Eigen::VectorXd& b1,
    Eigen::VectorXd& z1,
    std::mutex& mutex,
    std::condition_variable& cv_start,
    std::condition_variable& cv_done,
    int& current_epoch,
    int& completed,
    bool& stop)
{
    // Fijar el hilo a una CPU lógica
    pin_to_cpu(cpu);

    // Última época procesada por este hilo
    int local_epoch = -1;

    while (true)
    {
        // ----------------------------------------------------
        // Esperar una nueva época
        // ----------------------------------------------------

        std::unique_lock<std::mutex> lock(mutex);

        cv_start.wait(lock, [&]
        {
            return current_epoch > local_epoch || stop;
        });

        // ----------------------------------------------------
        // Finalizar hilo
        // ----------------------------------------------------

        if (stop)
        {
            break;
        }

        local_epoch = current_epoch;

        lock.unlock();

        // ----------------------------------------------------
        // Trabajo real de la MLP
        //
        // Cada hilo calcula una neurona diferente.
        // ----------------------------------------------------

        z1(neuron) =
            W1.row(neuron).dot(x) + b1(neuron);

        // ----------------------------------------------------
        // Avisar que este hilo terminó
        // ----------------------------------------------------

        lock.lock();

        completed++;

        if (completed == 2)
        {
            cv_done.notify_one();
        }
    }
}

// ============================================================
// Main
// ============================================================

int main()
{
    // ========================================================
    // Parámetros
    // ========================================================

    const double learning_rate = 0.1;
    const int epochs = 1000;

    auto start = std::chrono::steady_clock::now();

    // ========================================================
    // Entrada
    // ========================================================

    Eigen::VectorXd x(2);

    x << 0.5, 0.8;

    // Target
    double y = 1.0;

    // ========================================================
    // Pesos y sesgos
    // ========================================================

    Eigen::MatrixXd W1(2, 2);

    W1 << 0.1, 0.2,
          0.3, 0.4;

    Eigen::VectorXd b1(2);

    b1 << 0.1, 0.1;

    Eigen::MatrixXd W2(1, 2);

    W2 << 0.5, 0.6;

    double b2 = 0.1;

    // ========================================================
    // Variables compartidas por los workers
    // ========================================================

    std::mutex mutex;

    std::condition_variable cv_start;
    std::condition_variable cv_done;

    int current_epoch = -1;
    int completed = 0;

    bool stop = false;

    // Resultado de la capa oculta
    Eigen::VectorXd z1(2);

    // ========================================================
    // Crear los hilos UNA SOLA VEZ
    //
    // CPU 0 y CPU 1 pertenecen al mismo núcleo físico.
    //
    // Thread 0 -> neurona 0 -> CPU 0
    // Thread 1 -> neurona 1 -> CPU 1
    // ========================================================

    std::thread t0(
        compute_neuron,
        0,
        0,
        std::cref(W1),
        std::cref(x),
        std::cref(b1),
        std::ref(z1),
        std::ref(mutex),
        std::ref(cv_start),
        std::ref(cv_done),
        std::ref(current_epoch),
        std::ref(completed),
        std::ref(stop)
    );

    std::thread t1(
        compute_neuron,
        1,
        1,
        std::cref(W1),
        std::cref(x),
        std::cref(b1),
        std::ref(z1),
        std::ref(mutex),
        std::ref(cv_start),
        std::ref(cv_done),
        std::ref(current_epoch),
        std::ref(completed),
        std::ref(stop)
    );

    // ========================================================
    // Entrenamiento
    // ========================================================

    for (int epoch = 0; epoch < epochs; ++epoch)
    {
        // ====================================================
        // Iniciar nueva época para los workers
        // ====================================================

        {
            std::lock_guard<std::mutex> lock(mutex);

            completed = 0;
            current_epoch++;
        }

        cv_start.notify_all();

        // ====================================================
        // Esperar a que ambas neuronas terminen
        // ====================================================

        {
            std::unique_lock<std::mutex> lock(mutex);

            cv_done.wait(lock, [&]
            {
                return completed == 2;
            });
        }

        // ====================================================
        // FORWARD PASS
        // ====================================================

        // Activación de la capa oculta
        Eigen::VectorXd a1 = z1.unaryExpr(
            [](double value)
            {
                return sigmoid(value);
            }
        );

        // ----------------------------------------------------
        // Capa de salida
        // ----------------------------------------------------

        double z2 = (W2 * a1)(0) + b2;

        double a2 = sigmoid(z2);

        // ====================================================
        // ERROR
        // ====================================================

        double error = a2 - y;

        double mse = error * error;

        // ====================================================
        // BACKPROPAGATION
        // ====================================================

        // Derivada del MSE
        double dMSE_da2 = 2.0 * (a2 - y);

        // Derivada de sigmoid
        double da2_dz2 = a2 * (1.0 - a2);

        // Gradiente de z2
        double dz2 = dMSE_da2 * da2_dz2;

        // Gradiente W2
        Eigen::MatrixXd dW2 =
            dz2 * a1.transpose();

        // Gradiente b2
        double db2 = dz2;

        // Gradiente respecto a a1
        Eigen::VectorXd da1 =
            W2.transpose() * dz2;

        // Gradiente respecto a z1
        Eigen::VectorXd dz1 =
            da1.array()
            * a1.array()
            * (1.0 - a1.array());

        // Gradiente W1
        Eigen::MatrixXd dW1 =
            dz1 * x.transpose();

        // Gradiente b1
        Eigen::VectorXd db1 = dz1;

        // ====================================================
        // GRADIENT DESCENT
        // ====================================================

        W2 -= learning_rate * dW2;
        b2 -= learning_rate * db2;

        W1 -= learning_rate * dW1;
        b1 -= learning_rate * db1;

        // ====================================================
        // Mostrar progreso
        // ====================================================

        if (epoch % 100 == 0)
        {
            std::cout
                << "Epoch: " << epoch
                << " | MSE: " << mse
                << " | Prediction: " << a2
                << '\n';
        }
    }

    // ========================================================
    // Detener los workers
    // ========================================================

    {
        std::lock_guard<std::mutex> lock(mutex);

        stop = true;
    }

    cv_start.notify_all();

    // Esperar terminación
    t0.join();
    t1.join();

    // ========================================================
    // Resultados finales
    // ========================================================

    auto end = std::chrono::steady_clock::now();

    auto elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(
            end - start
        );

    cout << "\n=== SMT ===\n";

    cout << "\nTiempo total: "
        << elapsed.count()
        << " us\n";

    std::cout << "\nW1:\n"
              << W1 << '\n';

    std::cout << "\nb1:\n"
              << b1 << '\n';

    std::cout << "\nW2:\n"
              << W2 << '\n';

    std::cout << "\nb2: "
              << b2 << '\n';

    return 0;
}