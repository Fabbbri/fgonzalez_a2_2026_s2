#include <iostream>
#include <chrono>
#include <cstdlib>
#include "checksum.h"        
#include "mnist.h"

using std::cout;

using Eigen::VectorXd;
using Eigen::MatrixXd;
using Eigen::RowVectorXd;

VectorXd sigmoid(const VectorXd& z) {
    return 1.0 / (1.0 + (-z.array()).exp());
}

int main(int argc, char** argv) {

    // =========================================
    // CONFIGURACIÓN DEL ENTRENAMIENTO
    // =========================================

    // n (\eta indica que tanto avanza en la direccion del gradiente)
    double learning_rate = 0.1;
    const int num_muestras = 100;
    const int epochs = (argc > 1) ? std::atoi(argv[1]) : 1000;
    double mse_promedio = 0.0;
    auto start = std::chrono::steady_clock::now();
    int conteo_clases[10] = {0};

    Sample sample;

    // =========================================
    // PESOS Y SESGOS
    // =========================================

    MatrixXd W1 = MatrixXd::Random(128, 784) * 0.1;
    MatrixXd b1 = MatrixXd::Zero(128, 1);

    MatrixXd W2 = MatrixXd::Random(64, 128) * 0.1;
    MatrixXd b2 = MatrixXd::Zero(64, 1);

    MatrixXd W3 = MatrixXd::Random(10, 64) * 0.1;
    MatrixXd b3 = MatrixXd::Zero(10, 1);

    // Y_PRED , clase predicha que genera el modelo
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
    
    // =========================================
    // ENTRENAMIENTO VARIAS IMG
    // =========================================

    for (int epoch = 0; epoch<epochs ; epoch++){

        // acumulador de MSE para comparar entre imagenes
        double mse_total = 0.0;
        int aciertos = 0;
        Eigen::Index clase_real;
        Eigen::Index clase_predicha;

        for (int i=0 ; i<num_muestras ; i++){

            sample = load_sample(image_file, label_file, i);
            // sample.x es el vector de entrada
            // sample.y es la etiqueta (valor objetivo / clase real)
        
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
            mse_total += mse;

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

            // -----------------------------------------
            // Registrar aciertos de ESTA imagen
            // -----------------------------------------

            if (epoch == 0) {
                Eigen::Index clase_real;
                sample.y.maxCoeff(&clase_real);
                conteo_clases[clase_real]++;
            }

            // maxCoeff devuelve un numero y lo guarda en la direccion del argumento
            a3.maxCoeff(&clase_predicha);

            sample.y.maxCoeff(&clase_real);

            if (clase_predicha == clase_real) {
                aciertos++;
            }

        }

        // ===================================
        // Fin de las 100 imágenes 
        // ===================================

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

    uint64_t h = CHECKSUM_INICIAL;
    h = hash_params(W1, h); h = hash_params(b1, h);
    h = hash_params(W2, h); h = hash_params(b2, h);
    h = hash_params(W3, h); h = hash_params(b3, h);
    cout << "\nChecksum parametros: " << std::hex << h << std::dec << "\n";

    // Cuantas imágenes corresponden a cada número
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
    
    cout << "\n=== Secuencial ===\n";

    cout << "\nTiempo total: "
         << elapsed.count()
         << " us\n";

    return 0;
}