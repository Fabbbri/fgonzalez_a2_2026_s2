#include <iostream>
#include <chrono>
#include "mnist.h"

using std::cout;

using Eigen::VectorXd;
using Eigen::MatrixXd;
using Eigen::RowVectorXd;

VectorXd sigmoid(const VectorXd& z) {
    return 1.0 / (1.0 + (-z.array()).exp());
}

int main() {

    // =========================================
    // CONFIGURACIÓN DEL ENTRENAMIENTO
    // =========================================

    // n (\eta indica que tanto avanza en la direccion del gradiente)
    double learning_rate = 0.1;
    int epochs = 1000;
    auto start = std::chrono::steady_clock::now();

    // =========================================
    // PESOS Y SESGOS
    // =========================================

    MatrixXd W1 = MatrixXd::Random(128, 784) * 0.1;
    MatrixXd b1 = MatrixXd::Zero(128, 1);

    MatrixXd W2 = MatrixXd::Random(64, 128) * 0.1;
    MatrixXd b2 = MatrixXd::Zero(64, 1);

    MatrixXd W3 = MatrixXd::Random(10, 64) * 0.1;
    MatrixXd b3 = MatrixXd::Zero(10, 1);

    // Y_PRED que genera el modelo
    VectorXd a3 = MatrixXd::Zero(10, 1);

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

    Sample sample = load_sample(image_file, label_file, 5000);

    // sample.x es el vector de entrada
    // sample.y es la etiqueta (valor objetivo)
    cout << "X: " << sample.x.size() << " elementos\n";
    cout << "Y: " << sample.y.size() << " elementos\n";

    // =========================================
    // ENTRENAMIENTO (por ahora uno, luego epochs)
    // =========================================

    for (int epoch = 0; epoch<epochs ; epoch++){
    
        // Primer forward-pass a W1
        // z = función de activación
        VectorXd z1 = W1 * sample.x + b1;
    
        // Aplicar transformación sigmoid
        VectorXd a1 = sigmoid(z1);

        // Segundo forward-pass a W2
        VectorXd z2 = W2 * a1 + b2;

        // Aplicar transformación sigmoid
        VectorXd a2 = sigmoid(z2);

        // Tercer y último forward-pass a W3
        VectorXd z3 = W3 * a2 + b3;

        // Aplicar transformación sigmoid
        a3 = sigmoid(z3);

        // -----------------------------------------
        // LOSS FUNCTION: MSE
        // -----------------------------------------

        double mse = (a3 - sample.y).squaredNorm() / a3.size();

        // -----------------------------------------
        // Backpropagation - capa de salida
        // -----------------------------------------

        // Derivada del error respecto a A3
        VectorXd dA3 = (2.0 / a3.size()) * (a3 - sample.y);

        // Derivada de Z3
        VectorXd dZ3 = dA3.array() * a3.array() * (1.0 - a3.array());

        // Derivadas de W3 y b3
        MatrixXd dW3 = dZ3 * a2.transpose();
        VectorXd db3 = dZ3;

        // -----------------------------------------
        // Backpropagation - capas ocultas
        // -----------------------------------------

        // Derivada de A2
        VectorXd dA2 = W3.transpose() * dZ3;

        // Derivada de Z2 
        VectorXd dZ2 = dA2.array() * a2.array() * (1.0 - a2.array());

        // Derivada de W2 y b2
        MatrixXd dW2 = dZ2 * a1.transpose();
        VectorXd db2 = dZ2;

        // Derivada de A1
        VectorXd dA1 = W2.transpose() * dZ2;

        // Derivada de Z1
        VectorXd dZ1 = dA1.array() * a1.array() * (1.0 - a1.array());

        // Derivada de W1 y b1
        MatrixXd dW1 = dZ1 * sample.x.transpose();
        VectorXd db1 = dZ1;

        // -----------------------------------------
        // Descenso de gradiente -> Actualizar parámetros
        // -----------------------------------------

        W3 -= learning_rate * dW3;
        b3 -= learning_rate * db3;

        W2 -= learning_rate * dW2;
        b2 -= learning_rate * db2;

        W1 -= learning_rate * dW1;
        b1 -= learning_rate * db1;

        if (epoch % 100 == 0) {
            cout
                << "Epoch: " << epoch
                << " | MSE: " << mse
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
    
    cout << "\n=== Secuencial ===\n";

    cout << "\nTiempo total: "
         << elapsed.count()
         << " us\n";

    cout << "\nA3:\n" << a3 << "\n"; 
    cout << "\nValor Objetivo "; 
    cout << "Y:\n" << sample.y << "\n";

    return 0;
}