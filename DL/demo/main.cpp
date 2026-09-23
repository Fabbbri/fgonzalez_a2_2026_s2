#include <iostream>
#include <cmath>
#include <Eigen/Dense>

using std::cout;

using Eigen::Vector2d;
using Eigen::Matrix2d;
using Eigen::RowVector2d;

double sigmoid(double x) {
    return 1.0 / (1.0 + std::exp(-x));
}

Vector2d forwardPassVector2d(Matrix2d W, Vector2d x, Vector2d b){
    // Definir z = función de activación de la capa oculta
    Vector2d z = W * x + b;

    // Aplicarle una transformación a la función de Activación
    // Se usa sigmoid, también podría ser válido RELU
    Vector2d a;
    a(0) = sigmoid(z(0));
    a(1) = sigmoid(z(1));

    return a;
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

    cout << "\nValor Objetivo: y = 1\n";

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
        Vector2d a1 = forwardPassVector2d(W1, x, b1);

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
            cout
                << "Epoch: " << epoch
                << " | MSE: " << mse
                << " | Prediction: " << y_pred
                << '\n';
        }
    }

    // =========================================
    // RESULTADOS FINALES
    // =========================================

    cout << "\nPesos y sesgos finales:\n";
    cout << "W1:\n" << W1 << "\n\n";
    cout << "b1:\n" << b1 << "\n\n";

    cout << "W2:\n" << W2 << "\n\n";
    cout << "b2: " << b2 << "\n";

    return 0;
}