#include "interferogram/models/coarse_grained.hpp"
#include "interferogram/kernel.hpp"
#include <algorithm>
#include <stdexcept>
#include <system_error>
#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif

namespace interferogram {
/*
 * Crea los trabajadores y espera a que todos arranquen antes de regresar.
 * Si cpus no está vacío, el trabajador i queda fijado a cpus[i].
 *
 * @throws std::invalid_argument si threads no es positivo o cpus no tiene
 *         una CPU por trabajador.
 * @throws std::system_error / std::runtime_error si no se pudo fijar la afinidad.
 */
CoarseGrained::CoarseGrained(int width, int height, int tileWidth, int tileHeight, int threads,
                             const std::vector<int>& cpus, bool dynamicTiles)
    : width_(width), dynamicTiles_(dynamicTiles), samples_(0), tiles_(makeTiles(width, height, tileWidth, tileHeight)) {
    if (threads <= 0) throw std::invalid_argument("La cantidad de hilos debe ser positiva.");
    if (!cpus.empty() && cpus.size() != static_cast<std::size_t>(threads))
        throw std::invalid_argument("Una CPU por trabajador requerida.");
    samples_ = static_cast<std::size_t>(width) * height;
    threads_.reserve(threads);
    try {
        // Reparto estático: rangos contiguos; los primeros reciben un tile más.
        const std::size_t base = tiles_.size() / threads;
        const std::size_t extra = tiles_.size() % threads;
        for (int worker = 0; worker < threads; ++worker) {
            const std::size_t begin = worker * base + std::min<std::size_t>(worker, extra);
            const std::size_t end = begin + base + (static_cast<std::size_t>(worker) < extra);
            // -1 indica que el trabajador no se fija a ninguna CPU (coarse-grained).
            const int cpu = cpus.empty() ? -1 : cpus[worker];
            threads_.emplace_back([this, begin, end, cpu] { work(begin, end, cpu); });
        }
        // Esperar el arranque para excluirlo de la medición posterior.
        std::unique_lock<std::mutex> lock(mutex_);
        done_.wait(lock, [this] { return started_ == threads_.size(); });
        if (startupError_) std::rethrow_exception(startupError_);
    } catch (...) {
        // Recoger los hilos que sí arrancaron antes de propagar el error.
        stop();
        throw;
    }
}

CoarseGrained::~CoarseGrained() { 
    stop(); }

/*
 * Pide a los trabajadores que terminen y espera a que todos salgan.
 * También se usa si el constructor falla, para no dejar hilos sueltos.
 */
void CoarseGrained::stop() {
    {
        // Marcar el cierre bajo el mutex para que los trabajadores lo vean al despertar.
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
    }
    // Despertar a los trabajadores que esperan otro cálculo para que puedan salir.
    ready_.notify_all();
    // Esperar a que cada hilo termine; joinable evita unir dos veces.
    for (auto& thread : threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
}

// Ciclo de cada trabajador, fijar su CPU, avisar que arrancó y esperar trabajo.
void CoarseGrained::work(std::size_t begin, std::size_t end, int cpu) {
    std::exception_ptr error;
    try {
        if (cpu >= 0) {
#ifdef __linux__
            // cpu_set_t solo puede representar CPUs menores que CPU_SETSIZE.
            if (cpu >= CPU_SETSIZE) throw std::runtime_error("CPU fuera de rango");
            // Máscara con una sola CPU: el hilo solo podrá ejecutarse en ella.
            cpu_set_t mask;
            CPU_ZERO(&mask);
            CPU_SET(cpu, &mask);
            // Aplica la afinidad al hilo actual, no a todo el proceso.
            const int status = pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask);
            // pthread devuelve el código de error en lugar de usar errno.
            if (status) throw std::system_error(status, std::generic_category(), "Afinidad SMT");
            // Re-leer la afinidad: el sistema puede restringirla (cgroups, taskset).
            cpu_set_t actual;
            CPU_ZERO(&actual);
            const int readStatus = pthread_getaffinity_np(pthread_self(), sizeof(actual), &actual);
            if (readStatus) throw std::system_error(readStatus, std::generic_category(), "Verificar afinidad SMT");
            // Confirmar que el sistema aplicó la afinidad pedida.
            if (CPU_COUNT(&actual) != 1 || !CPU_ISSET(cpu, &actual))
                throw std::runtime_error("Afinidad SMT distinta de la solicitada");
#else
            throw std::runtime_error("Afinidad SMT requiere Linux");
#endif
        }
    } catch (...) { error = std::current_exception(); }

    // seen guarda la última generación procesada para no repetir un cálculo.
    std::size_t seen = 0;
    std::unique_lock<std::mutex> lock(mutex_);
    // Guardar el error para que el constructor lo relance en el hilo principal.
    if (error) startupError_ = error;
    // Avisar al constructor, que espera hasta que started_ == cantidad de hilos.
    ++started_;
    done_.notify_one();
    if (error) return; // Sin afinidad válida, este trabajador no participa.
    for (;;) {
        // Dormir hasta que compute() publique una generación nueva o se cierre el pool.
        ready_.wait(lock, [this, &seen] { return stopping_ || generation_ != seen; });
        if (stopping_) {
            return;
        }
        seen = generation_;
        const auto& a = *a_;
        const auto& b = *b_;
        auto& result = *result_;
        lock.unlock();
        // Regiones disjuntas: no se bloquea por píxel ni se comparte la escritura.
        // fetch_add da un único dueño a cada tile. La publicación de entradas y
        // la finalización se sincronizan con mutex_; el índice solo asigna trabajo.
        // Dinámico (SMT): cada trabajador toma el siguiente tile libre hasta agotarlos.
        // Estático (coarse): recorre su rango fijo [begin, end).
        for (std::size_t t = dynamicTiles_ ? nextTile_.fetch_add(1, std::memory_order_relaxed) : begin;
             t < (dynamicTiles_ ? tiles_.size() : end);
             t = dynamicTiles_ ? nextTile_.fetch_add(1, std::memory_order_relaxed) : t + 1) {
            const auto& tile = tiles_[t];
            // Recorrer el tile por filas; tile guarda coordenadas locales a la ventana.
            for (int y = tile.yBegin; y < tile.yEnd; ++y)
                for (int x = tile.xBegin; x < tile.xEnd; ++x) {
                    // Índice lineal en los vectores de la ventana (orden por filas).
                    const std::size_t i = static_cast<std::size_t>(y) * width_ + x;
                    result[i] = conjugateProduct(a[i], b[i]);
                }
        }
        // Volver a tomar el mutex para descontar este trabajador de forma segura.
        lock.lock();
        // El último en terminar despierta a compute(), que espera remaining_ == 0.
        if (--remaining_ == 0) {
            done_.notify_one();
        }
    }
}

/*
 * Calcula el interferograma completo con los trabajadores del pool.
 * Bloquea hasta que todos terminan; al regresar, result está completo.
 *
 * @throws std::invalid_argument si los vectores no cumplen el contrato del
 *         kernel o no tienen el tamaño de la ventana del pool.
 */
void CoarseGrained::compute(const std::vector<Complex>& a, const std::vector<Complex>& b,
                            std::vector<Complex>& result) {
    validateInterferogramInputs(a, b, result);
    if (a.size() != samples_) {
        throw std::invalid_argument("La ventana no coincide con el pool.");
    }
    // Publicar los datos y una nueva generación despierta a todos los trabajadores.
    std::unique_lock<std::mutex> lock(mutex_);
    // Compartir los vectores por puntero, sin copiarlos.
    a_ = &a;
    b_ = &b;
    result_ = &result;
    // Reiniciar la cola de tiles; solo se usa con reparto dinámico.
    nextTile_.store(0, std::memory_order_relaxed);
    // Todos los trabajadores participan, aunque alguno no tenga tiles.
    remaining_ = threads_.size();
    // Una generación nueva indica a cada trabajador que hay un cálculo pendiente.
    ++generation_;
    ready_.notify_all();
    // wait libera el mutex mientras espera, así los trabajadores pueden reportar.
    done_.wait(lock, [this] { return remaining_ == 0; });
    // Los vectores pertenecen al llamador; no conservar punteros después del cálculo.
    a_ = nullptr;
    b_ = nullptr;
    result_ = nullptr;
}
} // namespace interferogram
