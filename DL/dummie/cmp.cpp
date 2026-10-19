#include <Eigen/Dense>

#include <pthread.h>
#include <sched.h>

#include <condition_variable>
#include <cmath>
#include <iostream>
#include <mutex>
#include <thread>
#include <chrono>

// ============================================================
// Afinidad de CPU
// CPU 0 -> Core físico 0
// CPU 2 -> Core físico 1
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
    // Fijar este hilo a un núcleo físico específico
    pin_to_cpu(cpu);

    int local_epoch = -1;

    while (true)
    {
        std::unique_lock<std::mutex> lock(mutex);

        cv_start.wait(lock, [&]
        {
            return current_epoch > local_epoch || stop;
        });

        if (stop)
        {
            break;
        }

        local_epoch = current_epoch;

        lock.unlock();

        // ====================================================
        // Trabajo paralelo
        // Cada hilo calcula una neurona diferente
        // ====================================================

        z1(neuron) =
            W1.row(neuron).dot(x) + b1(neuron);

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
    const double learning_rate = 0.1;
    const int epochs = 1000;
    auto start = std::chrono::steady_clock::now();

    // ========================================================
    // Datos de entrada
    // ========================================================

    Eigen::VectorXd x(2);
    x << 0.5, 0.8;

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
    // Sincronización
    // ========================================================

    std::mutex mutex;

    std::condition_variable cv_start;
    std::condition_variable cv_done;

    int current_epoch = -1;
    int completed = 0;
    bool stop = false;

    Eigen::VectorXd z1(2);

    // ========================================================
    // Hilos CMP
    //
    // Hilo 0 -> CPU 0 -> Core físico 0
    // Hilo 1 -> CPU 2 -> Core físico 1
    //
    // Son núcleos físicos diferentes.
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
        2,
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
        // Preparar nueva época
        {
            std::lock_guard<std::mutex> lock(mutex);

            completed = 0;
            current_epoch++;
        }

        // Despertar ambos hilos
        cv_start.notify_all();

        // Esperar a que ambas neuronas terminen
        {
            std::unique_lock<std::mutex> lock(mutex);

            cv_done.wait(lock, [&]
            {
                return completed == 2;
            });
        }

        // ====================================================
        // Forward Pass
        // ====================================================

        Eigen::VectorXd a1 = z1.unaryExpr(
            [](double value)
            {
                return sigmoid(value);
            }
        );

        double z2 = (W2 * a1)(0) + b2;
        double a2 = sigmoid(z2);

        // ====================================================
        // Error
        // ====================================================

        double error = a2 - y;
        double mse = error * error;

        // ====================================================
        // Backward Pass
        // ====================================================

        double dMSE_da2 = 2.0 * (a2 - y);

        double da2_dz2 =
            a2 * (1.0 - a2);

        double dz2 =
            dMSE_da2 * da2_dz2;

        Eigen::MatrixXd dW2 =
            dz2 * a1.transpose();

        double db2 = dz2;

        Eigen::VectorXd da1 =
            W2.transpose() * dz2;

        Eigen::VectorXd dz1 =
            da1.array()
            * a1.array()
            * (1.0 - a1.array());

        Eigen::MatrixXd dW1 =
            dz1 * x.transpose();

        Eigen::VectorXd db1 = dz1;

        // ====================================================
        // Gradient Descent
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
    // Detener workers
    // ========================================================

    {
        std::lock_guard<std::mutex> lock(mutex);
        stop = true;
    }

    cv_start.notify_all();

    t0.join();
    t1.join();

    // ========================================================
    // Resultados
    // ========================================================

    auto end = std::chrono::steady_clock::now();

    auto elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(
            end - start
        );

    std::cout << "\n=== CMP ===\n";

    std::cout << "\nTiempo total: "
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