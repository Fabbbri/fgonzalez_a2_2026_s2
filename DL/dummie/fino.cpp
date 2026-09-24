#include <iostream>
#include <cmath>
#include <Eigen/Dense>
#include <chrono>

using std::cout;

using Eigen::Vector2d;
using Eigen::Matrix2d;
using Eigen::RowVector2d;

struct ThreadState {
    int i;       // neurona
    int j;       // siguiente entrada a procesar
    bool done;
};

void execute_quantum(
    ThreadState& state,
    const Matrix2d& W1,
    const Vector2d& x,
    Vector2d& z1,
    std::chrono::microseconds quantum
) {
    auto start = std::chrono::steady_clock::now();

    while (!state.done) {

        // Una operación de W1 * x
        z1(state.i) += W1(state.i, state.j) * x(state.j);

        state.j++;

        if (state.j == 2) {
            state.done = true;
        }

        auto now = std::chrono::steady_clock::now();

        if (now - start >= quantum) {
            break;
        }
    }
}


double sigmoid(double x) {
    return 1.0 / (1.0 + std::exp(-x));
}

int main() {

    // =========================================
    // CONFIGURACIÓN DEL ENTRENAMIENTO
    // =========================================

    // n (que tanto avanza en la direccion del gradiente)
    double learning_rate = 0.1;
    int epochs = 1000;
    // Valor objetivo
    double y = 1.0;
    const auto quantum = std::chrono::microseconds(1);
    int quantums = 0;
    int context_switches = 0;
    auto start = std::chrono::steady_clock::now();


    // =========================================
    // DATOS DE ENTRADA
    // =========================================

    Vector2d x;
    x << 0.5,
         0.8;

    // =========================================
    // PESOS Y SESGOS
    // =========================================

    // Capa oculta
    Matrix2d W1;
    W1 << 0.1, 0.2,
          0.3, 0.4;

    Vector2d b1;
    b1 << 0.1,
          0.1;

    // Capa de salida de una sola neurona
    RowVector2d W2;
    W2 << 0.5, 0.6;

    double b2 = 0.1;

    // =========================================
    // ENTRENAMIENTO
    // =========================================

    for (int epoch = 0; epoch<epochs ; epoch++){
        // -----------------------------------------
        // FORWARD PASS 
        // -----------------------------------------

        // Propagar Entrada -> Hidden Layer
        Vector2d z1;
        Vector2d a1;
        z1 = b1;

        ThreadState t0{0, 0, false};
        ThreadState t1{1, 0, false};

        int current_thread = 0;

        while (!t0.done || !t1.done) {

            ThreadState* current =
                (current_thread == 0) ? &t0 : &t1;

            if (!current->done) {

                execute_quantum(
                    *current, W1, x, z1, quantum
                );

                quantums++;

                // El hilo sigue teniendo trabajo pendiente:
                // significa que el quantum lo interrumpió.
                if (!current->done) {
                    context_switches++;
                }
            }

            current_thread = 1 - current_thread;
        }

        a1(0) = sigmoid(z1(0));
        a1(1) = sigmoid(z1(1));

        // Propagar Hidden Layer -> Salida
        double z2 = (W2 * a1)(0) + b2;

        double y_pred = sigmoid(z2);

        // -----------------------------------------
        // LOSS: MSE
        // -----------------------------------------

        // Error C: diferencia entre predicción y producido
        double error = y - y_pred;
        // MSE = error^2
        double mse = error * error;

        // -----------------------------------------
        // Backpropagation - capa de salida
        // -----------------------------------------

        // Derivada del error respecto al peso W2
        double dL_dypred = 2.0 * (y_pred - y);

        double dypred_dz2 = y_pred * (1.0 - y_pred);

        double dL_dz2 = dL_dypred * dypred_dz2;

        RowVector2d dW2 = dL_dz2 * a1.transpose();

        // Derivada del error respecto al sesgo b2
        double db2 = dL_dz2;


        // -----------------------------------------
        // Backpropagation - capa oculta
        // -----------------------------------------

        // Derivada del error respecto al peso W1
        Vector2d dL_da1 =
            W2.transpose() * dL_dz2;

        Vector2d sigmoid_derivative =
            a1.array() * (1.0 - a1.array());

        Vector2d dL_dz1 =
            dL_da1.array() * sigmoid_derivative.array();

        Matrix2d dW1 =
            dL_dz1 * x.transpose();

        // Derivada del error respecto al sesgo b1
        Vector2d db1 =
            dL_dz1;

        // -----------------------------------------
        // Descenso de gradiente -> Actualizar parámetros
        // -----------------------------------------

        W2 -= learning_rate * dW2;
        b2 -= learning_rate * db2;

        W1 -= learning_rate * dW1;
        b1 -= learning_rate * db1;

        if (epoch % 100 == 0) {
            cout << "\n=== Fine-Grained ===\n";
            cout << "Quantum: " << quantum.count() << " us\n";
            cout << "Quantums ejecutados: " << quantums << '\n';
            cout << "Cambios de contexto: " << context_switches << '\n';
            cout << "Epoch: " << epoch
                 << " | MSE: " << mse
                 << " | Prediction: " << y_pred
                 << '\n';
        }
    }

    // =========================================
    // RESULTADOS FINALES
    // =========================================

    auto end = std::chrono::steady_clock::now();

    auto elapsed =
        std::chrono::duration_cast<std::chrono::microseconds>(
            end - start
        );

    cout << "\n=== Hilo Fino (Intercalado) ===\n";

    cout << "\nTiempo total: "
         << elapsed.count()
         << " us\n";

    cout << "\nValor Objetivo: y = 1\n";

    cout << "\nPesos y sesgos finales:\n";
    cout << "W1:\n" << W1 << "\n\n";
    cout << "b1:\n" << b1 << "\n\n";

    cout << "W2:\n" << W2 << "\n\n";
    cout << "b2: " << b2 << "\n";

    return 0;
}