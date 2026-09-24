#include <iostream>
#include <cmath>
#include <thread>
#include <pthread.h>
#include <sched.h>

#include <Eigen/Dense>

using std::cout;

using Eigen::Vector2d;
using Eigen::Matrix2d;
using Eigen::RowVector2d;


// ============================================================
// Afinidad de CPU
// Ambos hilos serán ejecutados en el mismo CPU.
// Esto evita paralelismo físico entre ellos.
// ============================================================

void pin_to_cpu(int cpu)
{
    cpu_set_t cpuset;

    CPU_ZERO(&cpuset);
    CPU_SET(cpu, &cpuset);

    pthread_setaffinity_np(
        pthread_self(),
        sizeof(cpu_set_t),
        &cpuset
    );
}


// ============================================================
// Cálculo de una neurona de la capa oculta
// ============================================================

void calculate_neuron(
    int i,
    const Matrix2d& W1,
    const Vector2d& x,
    const Vector2d& b1,
    Vector2d& z1
) {
    pin_to_cpu(0);

    z1(i) = b1(i);

    for (int j = 0; j < 2; j++) {
        z1(i) += W1(i, j) * x(j);
    }
}


// ============================================================
// Sigmoide
// ============================================================

double sigmoid(double x)
{
    return 1.0 / (1.0 + std::exp(-x));
}


// ============================================================
// MAIN
// ============================================================

int main()
{
    // --------------------------------------------------------
    // Parámetros
    // --------------------------------------------------------

    const double learning_rate = 0.1;
    const int epochs = 1000;

    const double y = 1.0;


    // --------------------------------------------------------
    // Datos de entrada
    // --------------------------------------------------------

    Vector2d x;
    x << 0.5, 0.8;


    // --------------------------------------------------------
    // Pesos y sesgos iniciales
    // --------------------------------------------------------

    Matrix2d W1;
    W1 << 0.1, 0.2,
          0.3, 0.4;

    Vector2d b1;
    b1 << 0.1, 0.1;

    RowVector2d W2;
    W2 << 0.5, 0.6;

    double b2 = 0.1;


    // --------------------------------------------------------
    // Entrenamiento
    // --------------------------------------------------------

    for (int epoch = 0; epoch < epochs; epoch++)
    {
        // ====================================================
        // FORWARD PASS
        // ====================================================

        Vector2d z1;

        // ----------------------------------------------------
        // W1 * x + b1
        //
        // T0 calcula z1(0)
        // T1 calcula z1(1)
        // ----------------------------------------------------

        std::thread t0(
            calculate_neuron,
            0,
            std::ref(W1),
            std::ref(x),
            std::ref(b1),
            std::ref(z1)
        );

        std::thread t1(
            calculate_neuron,
            1,
            std::ref(W1),
            std::ref(x),
            std::ref(b1),
            std::ref(z1)
        );

        // Esperamos a que ambos terminen.
        t0.join();
        t1.join();


        // ----------------------------------------------------
        // Activación de la capa oculta
        // ----------------------------------------------------

        Vector2d a1;

        a1(0) = sigmoid(z1(0));
        a1(1) = sigmoid(z1(1));


        // ----------------------------------------------------
        // Capa de salida
        // ----------------------------------------------------

        double z2 = (W2 * a1)(0) + b2;

        double y_pred = sigmoid(z2);


        // ====================================================
        // ERROR
        // ====================================================

        double error = y_pred - y;

        double mse = error * error;


        // ====================================================
        // BACKPROPAGATION
        // ====================================================

        // ----------------------------------------------------
        // Salida
        // ----------------------------------------------------

        double dL_dypred = 2.0 * (y_pred - y);

        double dypred_dz2 =
            y_pred * (1.0 - y_pred);

        double dL_dz2 =
            dL_dypred * dypred_dz2;


        // ----------------------------------------------------
        // Gradientes de W2 y b2
        // ----------------------------------------------------

        RowVector2d dW2 =
            dL_dz2 * a1.transpose();

        double db2 = dL_dz2;


        // ----------------------------------------------------
        // Gradientes de la capa oculta
        // ----------------------------------------------------

        Vector2d dL_da1 =
            W2.transpose() * dL_dz2;

        Vector2d sigmoid_derivative =
            a1.array() * (1.0 - a1.array());

        Vector2d dL_dz1 =
            dL_da1.array()
            * sigmoid_derivative.array();


        // ----------------------------------------------------
        // Gradientes de W1 y b1
        // ----------------------------------------------------

        Matrix2d dW1 =
            dL_dz1 * x.transpose();

        Vector2d db1 =
            dL_dz1;


        // ====================================================
        // GRADIENT DESCENT
        // ====================================================

        W2 -= learning_rate * dW2;
        b2 -= learning_rate * db2;

        W1 -= learning_rate * dW1;
        b1 -= learning_rate * db1;


        // ====================================================
        // INFORMACIÓN
        // ====================================================

        if (epoch % 100 == 0)
        {
            cout << "Epoch: " << epoch
                 << " | MSE: " << mse
                 << " | Prediction: " << y_pred
                 << '\n';
        }
    }


    // ========================================================
    // RESULTADOS FINALES
    // ========================================================

    cout << "\n=== Coarse-Grained ===\n";

    cout << "Modelo: 2 -> 2 -> 1\n";
    cout << "Hilos: 2\n";
    cout << "CPUs utilizadas: 1\n";

    cout << "\nW1:\n" << W1 << '\n';

    cout << "\nb1:\n" << b1 << '\n';

    cout << "\nW2:\n" << W2 << '\n';

    cout << "\nb2: " << b2 << '\n';

    return 0;
}