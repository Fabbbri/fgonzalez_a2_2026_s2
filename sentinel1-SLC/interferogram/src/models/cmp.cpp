#include "interferogram/models/cmp.hpp"
#include "interferogram/kernel.hpp"
#include <algorithm>
#include <stdexcept>
#include <limits>
#include <system_error>
#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif

namespace interferogram {
Cmp::Cmp(int width, int height, int tileWidth, int tileHeight, int threads)
    : width_(width), samples_(0), tiles_(makeTiles(width, height, tileWidth, tileHeight)) {
    if (width <= 0 || height <= 0 || threads < 0) {
        throw std::invalid_argument("Dimensiones o cantidad de hilos inválidas para CMP.");
    }
    if (static_cast<std::size_t>(width) > std::numeric_limits<std::size_t>::max() / height) {
        throw std::invalid_argument("Ventana demasiado grande para CMP.");
    }
    samples_ = static_cast<std::size_t>(width) * height;
    // La lista contiene una CPU lógica por núcleo.
    cpus_ = detectCpuInfo().coreCpus;
    // Evitar trabajadores sin muestras cuando la ventana es muy pequeña.
    const std::size_t count = threads == 0 ? std::min(cpus_.size(), samples_)
                                           : static_cast<std::size_t>(threads);
    if (count == 0 || count > cpus_.size() || count > samples_) {
        throw std::invalid_argument("CMP requiere entre 1 y min(núcleos físicos, muestras) hilos.");
    }
    cpus_.resize(count);
    threads = static_cast<int>(count);
    threads_.reserve(threads);
    try {
        // Mismo reparto de tiles que CoarseGrained; algunos trabajadores pueden quedar sin tiles.
        const std::size_t base = tiles_.size() / threads;
        const std::size_t extra = tiles_.size() % threads;
        for (int worker = 0; worker < threads; ++worker) {
            const std::size_t begin = worker * base + std::min<std::size_t>(worker, extra);
            const std::size_t end = begin + base + (static_cast<std::size_t>(worker) < extra);
            const int cpu = cpus_[worker];
            threads_.emplace_back([this, begin, end, cpu] { work(begin, end, cpu); });
        }
        // Esperar el arranque para excluirlo de la medición posterior.
        std::unique_lock<std::mutex> lock(mutex_);
        done_.wait(lock, [this] { return started_ == threads_.size(); });
        if (startupError_) {
            std::rethrow_exception(startupError_);
        }
    } catch (...) {
        // También recoger los hilos que sí arrancaron si falla la preparación.
        stop();
        throw;
    }
}

Cmp::~Cmp() {
    stop();
}

void Cmp::stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    // Despertar a los trabajadores que esperan otro cálculo para que puedan salir.
    ready_.notify_all();
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

void Cmp::work(std::size_t begin, std::size_t end, int cpu) {
    // La afinidad se establece antes de avisar que el trabajador está listo.
    std::exception_ptr error;
    try {
#ifdef __linux__
        cpu_set_t mask;
        CPU_ZERO(&mask);
        CPU_SET(cpu, &mask);
        const int status = pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask);
        if (status != 0) {
            throw std::system_error(status, std::generic_category(), "Afinidad CMP");
        }
#else
        (void)cpu;
        throw std::runtime_error("CMP con afinidad requiere Linux.");
#endif
    } catch (...) {
        // El constructor recibe el error; una excepción no debe escapar del hilo.
        error = std::current_exception();
    }
    std::size_t seen = 0;
    std::unique_lock<std::mutex> lock(mutex_);
    if (error) {
        startupError_ = error;
    }
    ++started_;
    done_.notify_one();
    if (error) {
        return;
    }
    for (;;) {
        // Cada llamada a compute cambia la generación: procesamos cada una una sola vez.
        ready_.wait(lock, [this, &seen] { return stopping_ || generation_ != seen; });
        if (stopping_) {
            return;
        }
        seen = generation_;
        const auto& a = *a_;
        const auto& b = *b_;
        auto& result = *result_;
        lock.unlock();
        // Cada muestra pertenece a un solo bloque; no hace falta bloquear la escritura.
        for (std::size_t t = begin; t < end; ++t) {
            const auto& tile = tiles_[t];
            for (int y = tile.yBegin; y < tile.yEnd; ++y) {
                for (int x = tile.xBegin; x < tile.xEnd; ++x) {
                    const std::size_t i = static_cast<std::size_t>(y) * width_ + x;
                    result[i] = conjugateProduct(a[i], b[i]);
                }
            }
        }
        lock.lock();
        // El último trabajador avisa al coordinador que el resultado está completo.
        if (--remaining_ == 0) {
            done_.notify_one();
        }
    }
}

void Cmp::compute(const std::vector<Complex>& a, const std::vector<Complex>& b,
                            std::vector<Complex>& result) {
    validateInterferogramInputs(a, b, result);
    if (a.size() != samples_) {
        throw std::invalid_argument("La ventana no coincide con el pool.");
    }
    std::unique_lock<std::mutex> lock(mutex_);
    // Compartimos los vectores sin copiarlos; permanecen en uso hasta terminar la espera.
    a_ = &a;
    b_ = &b;
    result_ = &result;
    remaining_ = threads_.size();
    ++generation_;
    ready_.notify_all();
    done_.wait(lock, [this] { return remaining_ == 0; });
    a_ = nullptr;
    b_ = nullptr;
    result_ = nullptr;
}
} // namespace interferogram
