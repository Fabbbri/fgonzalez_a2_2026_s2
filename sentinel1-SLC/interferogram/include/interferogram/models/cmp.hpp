#pragma once
#include "interferogram/interferogram.hpp"
#include "interferogram/cpu_info.hpp"
#include <exception>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace interferogram {

// Un bloque contiguo por trabajador, fijado a un núcleo físico diferente.
class Cmp {
public:
    // threads == 0 selecciona los núcleos disponibles, limitado por las muestras.
    Cmp(int width, int height, int threads = 0);
    ~Cmp();
    Cmp(const Cmp&) = delete;
    Cmp& operator=(const Cmp&) = delete;
    // result debe estar dimensionado. Regresa cuando todos los bloques están listos.
    // Las llamadas sobre este modelo deben hacerse una después de otra.
    void compute(const std::vector<Complex>& a, const std::vector<Complex>& b,
                 std::vector<Complex>& result);

    std::size_t workers() const {
        return threads_.size();
    }

    // IDs de CPUs lógicas asignadas, en el mismo orden que los trabajadores.
    const std::vector<int>& cpus() const {
        return cpus_;
    }

private:
    void work(std::size_t begin, std::size_t end, int cpu);
    void stop();
    std::size_t samples_;
    std::vector<int> cpus_;
    std::exception_ptr startupError_;
    std::vector<std::thread> threads_;
    std::mutex mutex_;
    // ready_ anuncia trabajo o cierre; done_ señala arranque y finalización.
    std::condition_variable ready_;
    std::condition_variable done_;
    bool stopping_ = false;
    std::size_t generation_ = 0;
    std::size_t remaining_ = 0;
    std::size_t started_ = 0;
    const std::vector<Complex>* a_ = nullptr;
    const std::vector<Complex>* b_ = nullptr;
    std::vector<Complex>* result_ = nullptr;
};
} // namespace interferogram
