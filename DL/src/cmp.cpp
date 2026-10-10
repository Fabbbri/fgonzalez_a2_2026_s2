// =============================================================
// cmp.cpp  -  Multiprocesamiento multinúcleo (CMP)
//
// Algoritmo: idéntico a smt.cpp / fino.cpp / main.cpp
// (SGD por imagen, mismo orden de muestras, misma inicialización,
// sigmoide + MSE). Las imágenes se procesan secuencialmente; dentro
// de cada imagen el trabajo independiente se reparte entre T hilos
// del sistema operativo con barreras de sincronización.
//
// Diferencia con smt.cpp: la COLOCACIÓN de los hilos.
//   - cmp: exactamente UN hilo por NÚCLEO FÍSICO (nunca dos hilos
//          en el mismo núcleo). Se elige automáticamente leyendo
//          la topología de /sys y se rechaza T > núcleos físicos.
//   - smt: los hilos pueden compartir núcleo (CPUs lógicas hermanas).
//
// Uso:   ./cmp [num_hilos] [epocas]
//   ./cmp 4        -> 4 hilos en 4 núcleos físicos distintos
//   ./cmp 8 20     -> 8 hilos, 20 épocas
// =============================================================

#include <iostream>
#include <chrono>
#include <cmath>
#include <vector>
#include <map>
#include <utility>
#include <fstream>
#include <cstdlib>
#include <cstdint>
#include <thread>
#include <atomic>
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
// CONFIGURACIÓN (igual que main.cpp / fino.cpp)
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
                else std::this_thread::yield();
            }
        }
    }

private:
    const int n_;
    std::atomic<int> cuenta_{0};
    std::atomic<int> fase_{0};
};

// Partición contigua y determinista de [0, n) para el hilo t de T.
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
// TRABAJADOR (mismo algoritmo que smt.cpp)
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

            // ---------- Métricas + delta de salida (hilo 0) ----------
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
// TOPOLOGÍA: UNA CPU LÓGICA POR NÚCLEO FÍSICO
// =========================================

// Lee un entero de un archivo de /sys; devuelve -1 si no se puede.
static int leer_entero(const std::string& ruta) {
    std::ifstream f(ruta);
    int v = -1;
    if (!(f >> v)) return -1;
    return v;
}

// Devuelve, para cada núcleo físico (socket, core_id), su CPU lógica
// de menor número. El mapa queda ordenado por (socket, core_id).
static std::map<std::pair<int, int>, int> nucleos_fisicos() {
    std::map<std::pair<int, int>, int> nucleos;
    const std::string base = "/sys/devices/system/cpu/cpu";
    unsigned total = std::thread::hardware_concurrency();
    if (total == 0) total = 64;

    for (unsigned cpu = 0; cpu < total; cpu++) {
        std::string dir = base + std::to_string(cpu) + "/topology/";
        int core = leer_entero(dir + "core_id");
        int pkg = leer_entero(dir + "physical_package_id");
        if (core < 0 || pkg < 0) continue;
        auto clave = std::make_pair(pkg, core);
        if (nucleos.find(clave) == nucleos.end()) {
            nucleos[clave] = static_cast<int>(cpu);  // primera CPU vista = menor número
        }
    }
    return nucleos;
}

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
#endif
}

// =========================================
// MAIN
// =========================================

int main(int argc, char** argv) {
    int T = (argc > 1) ? std::atoi(argv[1]) : 2;
    if (argc > 2) EPOCHS = std::atoi(argv[2]);

    if (T < 1 || T > 64 || EPOCHS < 1) {
        std::cerr << "Uso: ./cmp [num_hilos 1..64] [epocas >= 1]\n";
        return 1;
    }

    // CMP exige un hilo por núcleo físico: si no podemos garantizarlo, no ejecutamos.
    auto nucleos = nucleos_fisicos();
    if (nucleos.empty()) {
        std::cerr << "No se pudo leer la topologia en /sys; no se puede garantizar CMP.\n";
        return 1;
    }
    if (T > static_cast<int>(nucleos.size())) {
        std::cerr << "Error: " << T << " hilos > " << nucleos.size()
                  << " nucleos fisicos. Eso seria SMT/sobresuscripcion, no CMP.\n";
        return 1;
    }

    std::vector<int> cpus;
    for (const auto& par : nucleos) {
        if (static_cast<int>(cpus.size()) == T) break;
        cpus.push_back(par.second);
    }

    // Misma inicialización que fino.cpp / smt.cpp (mismo orden de llamadas a Random)
    std::srand(42);
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

    cout << "Hilos: " << T << " (1 por nucleo fisico) | CPUs: ";
    for (size_t i = 0; i < cpus.size(); i++) cout << (i ? "," : "") << cpus[i];
    cout << "\n";

    std::vector<std::thread> hilos;
    for (int t = 0; t < T; t++) {
        hilos.emplace_back(trabajador, t, T);
        fijar_cpu(hilos.back(), cpus[t]);
    }

    // Se mide una vez que todos los hilos están creados y fijados
    auto start = std::chrono::steady_clock::now();
    g_go.store(true, std::memory_order_release);

    for (auto& h : hilos) h.join();
    auto end = std::chrono::steady_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    uint64_t h = CHECKSUM_INICIAL;
    h = hash_params(g_modelo.W1, h); h = hash_params(g_modelo.b1, h);
    h = hash_params(g_modelo.W2, h); h = hash_params(g_modelo.b2, h);
    h = hash_params(g_modelo.W3, h); h = hash_params(g_modelo.b3, h);
    cout << "\nChecksum parametros: " << std::hex << h << std::dec << "\n";

    cout << "\n=== CMP ===\n";
    cout << "Tiempo total: " << elapsed.count() << " us\n";
    return 0;
}