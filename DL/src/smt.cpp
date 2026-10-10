// =============================================================
// smt.cpp  -  Aproximación SMT por sobresuscripción de hilos
//
// Algoritmo: idéntico a main.cpp / fino.cpp (SGD por imagen,
// mismo orden de muestras, misma inicialización, sigmoide + MSE).
//
// Estrategia: las muestras se procesan SECUENCIALMENTE (se respeta
// la dependencia entre imágenes), pero DENTRO de cada imagen el
// trabajo independiente se reparte entre T hilos persistentes,
// fijados a procesadores lógicos concretos (afinidad) y
// sincronizados con barreras.
//
// Uso:   
//   ./smt [num_hilos] [lista_cpus]
//   ./smt 2 0,8      -> 2 hilos en los lógicos 0 y 8
//                       (hermanos SMT si lscpu -e lo confirma)
//   ./smt 8 0,1,2,3,4,5,6,7
//   ./smt 16 0,1,...,15
//   ./smt 4          -> sin afinidad (decide el SO)
// =============================================================

#include <iostream>
#include <chrono>
#include <cmath>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <thread>
#include <atomic>
#include <sstream>
#include <string>
#include "mnist.h"
#include "checksum.h"

#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif

#if defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif

using std::cout;
using Eigen::VectorXd;
using Eigen::MatrixXd;

// =========================================
// CONFIGURACIÓN (igual que main.cpp)
// =========================================

static const double LEARNING_RATE = 0.1;
static const int NUM_MUESTRAS = 100;
static int EPOCHS = 1000;

// =========================================
// MODELO Y ESTADO COMPARTIDO
// =========================================

struct Modelo {
    MatrixXd W1, W2, W3;
    VectorXd b1, b2, b3;
};

struct Estado {
    VectorXd a1, a2, a3;
    VectorXd dZ3, dZ2, dZ1;
};

// =========================================
// UTILIDADES
// =========================================

static inline void cpu_relax() {
#if defined(__x86_64__) || defined(__i386__)
    _mm_pause();
#elif defined(__aarch64__)
    asm volatile("yield");
#endif
}

static double sigmoid(double x) {
    return 1.0 / (1.0 + std::exp(-x));
}

// Barrera de espera activa (C++17 no tiene std::barrier).
// Con espera activa los hilos no se duermen: así el coste de
// sincronización no se mezcla con cambios de contexto del SO.
// Ojo: un hilo que gira en la barrera también consume recursos
// del núcleo que comparte con su hermano SMT.
class Barrera {
public:
    explicit Barrera(int n) : n_(n) {}

    void esperar() {
        int fase_local = fase_.load(std::memory_order_acquire);
        if (cuenta_.fetch_add(1, std::memory_order_acq_rel) + 1 == n_) {
            cuenta_.store(0, std::memory_order_relaxed);
            fase_.store(fase_local + 1, std::memory_order_release);
        } else {
            int giros = 0;
            while (fase_.load(std::memory_order_acquire) == fase_local) {
                if (++giros < 4000) cpu_relax();
                else std::this_thread::yield();  // red de seguridad si hay sobresuscripción de CPUs lógicas
            }
        }
    }

private:
    const int n_;
    std::atomic<int> cuenta_{0};
    std::atomic<int> fase_{0};
};

// Partición contigua y determinista de [0, n) para el hilo t de T.
// IMPORTANTE: la misma función se usa en todas las etapas que tocan
// la misma fila/columna, así el dueño de un elemento siempre es el mismo.
static inline void rango(int n, int t, int T, int& ini, int& fin) {
    ini = (n * t) / T;
    fin = (n * (t + 1)) / T;
}

// =========================================
// VARIABLES GLOBALES COMPARTIDAS
// =========================================

static Modelo g_modelo;
static Estado g_est;
static std::vector<Sample> g_muestras;
static Barrera* g_barrera = nullptr;
static std::atomic<bool> g_go{false};

// =========================================
// TRABAJADOR
// =========================================

static void trabajador(int t, int T) {
    Modelo& m = g_modelo;
    Estado& e = g_est;
    const double lr = LEARNING_RATE;

    int f1_i, f1_f;  // 128 filas/columnas (capa 1)
    int f2_i, f2_f;  // 64 (capa 2)
    int f3_i, f3_f;  // 10 (salida)
    rango(128, t, T, f1_i, f1_f);
    rango(64, t, T, f2_i, f2_f);
    rango(10, t, T, f3_i, f3_f);

    // Métricas: solo las usa el hilo 0
    double mse = 0.0;
    int aciertos = 0;

    while (!g_go.load(std::memory_order_acquire)) std::this_thread::yield();

    for (int epoch = 0; epoch < EPOCHS; epoch++) {
        if (t == 0) { mse = 0.0; aciertos = 0; }

        for (int s = 0; s < NUM_MUESTRAS; s++) {
            const Sample& muestra = g_muestras[s];
            const VectorXd& x = muestra.x;

            // ---------- FORWARD capa 1 (filas de W1) ----------
            for (int i = f1_i; i < f1_f; i++) {
                double z = m.W1.row(i).dot(x) + m.b1(i);
                e.a1(i) = sigmoid(z);
            }
            g_barrera->esperar();

            // ---------- FORWARD capa 2 (filas de W2) ----------
            for (int i = f2_i; i < f2_f; i++) {
                double z = m.W2.row(i).dot(e.a1) + m.b2(i);
                e.a2(i) = sigmoid(z);
            }
            g_barrera->esperar();

            // ---------- FORWARD capa 3 (filas de W3) ----------
            for (int i = f3_i; i < f3_f; i++) {
                double z = m.W3.row(i).dot(e.a2) + m.b3(i);
                e.a3(i) = sigmoid(z);
            }
            g_barrera->esperar();

            // ---------- Métricas + delta de salida (trabajo diminuto: hilo 0) ----------
            if (t == 0) {
                mse += (e.a3 - muestra.y).squaredNorm() / 10.0;

                Eigen::Index real, pred;
                muestra.y.maxCoeff(&real);
                e.a3.maxCoeff(&pred);
                if (real == pred) aciertos++;

                for (int i = 0; i < 10; i++) {
                    double dA3 = (2.0 / 10.0) * (e.a3(i) - muestra.y(i));
                    e.dZ3(i) = dA3 * e.a3(i) * (1.0 - e.a3(i));
                    m.b3(i) -= lr * e.dZ3(i);
                }
            }
            g_barrera->esperar();

            // ---------- Capa 2: dueño de la COLUMNA j de W3 ----------
            // Lee la columna j ANTIGUA de W3 para dA2 y luego la actualiza:
            // nadie más toca esa columna, así no hace falta otra barrera
            // y se conserva la semántica "gradientes con pesos viejos".
            for (int j = f2_i; j < f2_f; j++) {
                double dA2 = m.W3.col(j).dot(e.dZ3);
                e.dZ2(j) = dA2 * e.a2(j) * (1.0 - e.a2(j));
                for (int i = 0; i < 10; i++) {
                    m.W3(i, j) -= lr * (e.dZ3(i) * e.a2(j));
                }
                m.b2(j) -= lr * e.dZ2(j);
            }
            g_barrera->esperar();

            // ---------- Capa 1: dueño de la COLUMNA k de W2 ----------
            for (int k = f1_i; k < f1_f; k++) {
                double dA1 = m.W2.col(k).dot(e.dZ2);
                e.dZ1(k) = dA1 * e.a1(k) * (1.0 - e.a1(k));
                for (int i = 0; i < 64; i++) {
                    m.W2(i, k) -= lr * (e.dZ2(i) * e.a1(k));
                }
                m.b1(k) -= lr * e.dZ1(k);
            }
            g_barrera->esperar();

            // ---------- Actualizar W1 (filas; mismo dueño que en el forward) ----------
            // Sin barrera final: la siguiente muestra solo vuelve a leer
            // las filas de W1 que este mismo hilo posee.
            for (int i = f1_i; i < f1_f; i++) {
                for (int j = 0; j < 784; j++) {
                    m.W1(i, j) -= lr * (e.dZ1(i) * x(j));
                }
            }
        }

        if (t == 0) {
            cout << "Epoch: " << epoch
                 << " | MSE: " << mse / NUM_MUESTRAS
                 << " | Accuracy: " << 100.0 * aciertos / NUM_MUESTRAS << "%\n";
        }
    }
}

// =========================================
// AFINIDAD
// =========================================

static void fijar_cpu(std::thread& th, int cpu) {
#ifdef __linux__
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    int rc = pthread_setaffinity_np(th.native_handle(), sizeof(set), &set);
    if (rc != 0) {
        std::cerr << "Aviso: no se pudo fijar el hilo a la CPU " << cpu
                  << " (error " << rc << ")\n";
    }
#else
    (void)th; (void)cpu;
    std::cerr << "Aviso: afinidad no soportada en este SO\n";
#endif
}

static std::vector<int> parsear_cpus(const std::string& lista) {
    std::vector<int> cpus;
    std::stringstream ss(lista);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        if (!tok.empty()) cpus.push_back(std::stoi(tok));
    }
    return cpus;
}

// =========================================
// CHECKSUM de parámetros (para comparar con fino/main)
// =========================================

static uint64_t checksum(const Modelo& m) {
    uint64_t h = 1469598103934665603ULL;
    h = hash_bits(m.W1.data(), m.W1.size(), h);
    h = hash_bits(m.b1.data(), m.b1.size(), h);
    h = hash_bits(m.W2.data(), m.W2.size(), h);
    h = hash_bits(m.b2.data(), m.b2.size(), h);
    h = hash_bits(m.W3.data(), m.W3.size(), h);
    h = hash_bits(m.b3.data(), m.b3.size(), h);
    return h;
}

// =========================================
// MAIN
// =========================================

int main(int argc, char** argv) {
    int T = (argc > 1) ? std::atoi(argv[1]) : 2;
    std::vector<int> cpus;
    if (argc > 2) cpus = parsear_cpus(argv[2]);
    if (argc > 3) EPOCHS = std::atoi(argv[3]);
    if (T < 1 || T > 64) {
        std::cerr << "num_hilos debe estar entre 1 y 64\n";
        return 1;
    }

    // Misma inicialización (mismo orden de llamadas a Random)
    g_modelo.W1 = MatrixXd::Random(128, 784) * 0.1;
    g_modelo.b1 = VectorXd::Zero(128);
    g_modelo.W2 = MatrixXd::Random(64, 128) * 0.1;
    g_modelo.b2 = VectorXd::Zero(64);
    g_modelo.W3 = MatrixXd::Random(10, 64) * 0.1;
    g_modelo.b3 = VectorXd::Zero(10);

    g_est.a1 = VectorXd::Zero(128);
    g_est.a2 = VectorXd::Zero(64);
    g_est.a3 = VectorXd::Zero(10);
    g_est.dZ1 = VectorXd::Zero(128);
    g_est.dZ2 = VectorXd::Zero(64);
    g_est.dZ3 = VectorXd::Zero(10);

    std::ifstream image_file("../data/mnist/train-images-idx3-ubyte", std::ios::binary);
    std::ifstream label_file("../data/mnist/train-labels-idx1-ubyte", std::ios::binary);
    if (!image_file || !label_file) {
        std::cerr << "No se pudieron abrir los archivos MNIST\n";
        return 1;
    }
    for (int i = 0; i < NUM_MUESTRAS; i++) {
        g_muestras.push_back(load_sample(image_file, label_file, i));
    }

    Barrera barrera(T);
    g_barrera = &barrera;

    cout << "Hilos: " << T << " | CPUs: ";
    if (cpus.empty()) cout << "(sin afinidad)";
    for (size_t i = 0; i < cpus.size(); i++) cout << (i ? "," : "") << cpus[i];
    cout << "\n";

    std::vector<std::thread> hilos;
    for (int t = 0; t < T; t++) {
        hilos.emplace_back(trabajador, t, T);
        if (!cpus.empty()) fijar_cpu(hilos.back(), cpus[t % cpus.size()]);
    }

    // Se mide una vez que todos los hilos están creados y fijados
    auto start = std::chrono::steady_clock::now();
    g_go.store(true, std::memory_order_release);

    for (auto& h : hilos) h.join();
    auto end = std::chrono::steady_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    cout << "\nChecksum parametros: " << std::hex << checksum(g_modelo) << std::dec << "\n";
    cout << "\n=== SMT ===\n";
    cout << "Tiempo total: " << elapsed.count() << " us\n";
    return 0;
}