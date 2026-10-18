#ifndef DL_RUNTIME_H
#define DL_RUNTIME_H
#include <Eigen/Dense>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <thread>
#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif
namespace dl {
inline int integer(const char* text, int minimum = 1) {
    std::string value(text); size_t used = 0;
    long n = std::stol(value, &used);
    if (used != value.size() || n < minimum || n > std::numeric_limits<int>::max())
        throw std::invalid_argument("Entero fuera de rango: " + value);
    return static_cast<int>(n);
}
inline int env_int(const char* key, int fallback, int minimum = 1) {
    const char* value = std::getenv(key);
    return value ? integer(value, minimum) : fallback;
}
inline bool quiet() { return std::getenv("DL_QUIET") && std::string(std::getenv("DL_QUIET")) == "1"; }
inline void initialize() {
    std::srand(env_int("DL_SEED", 1, 0));
    Eigen::setNbThreads(1);
    if (quiet()) std::cout.setstate(std::ios::failbit);
}
inline std::vector<std::vector<int>> cores() {
#ifdef __linux__
    cpu_set_t mask; CPU_ZERO(&mask);
    if (sched_getaffinity(0, sizeof(mask), &mask)) throw std::runtime_error("No se pudo consultar afinidad");
    std::map<std::pair<int,int>, std::vector<int>> groups;
    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) if (CPU_ISSET(cpu, &mask)) {
        const std::string base = "/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/topology/";
        int socket, core;
        if (!(std::ifstream(base + "physical_package_id") >> socket) ||
            !(std::ifstream(base + "core_id") >> core)) throw std::runtime_error("Topología incompleta");
        groups[{socket,core}].push_back(cpu);
    }
    std::vector<std::vector<int>> result;
    for (const auto& group : groups) result.push_back(group.second);
    if (result.empty()) throw std::runtime_error("No hay núcleos permitidos");
    return result;
#else
    throw std::runtime_error("CMP/SMT con afinidad requieren Linux");
#endif
}
inline std::vector<int> cpu_order() {
    const auto groups = cores(); std::vector<int> result;
    for (size_t sibling = 0;; ++sibling) {
        const auto before = result.size();
        for (const auto& group : groups) if (sibling < group.size()) result.push_back(group[sibling]);
        if (before == result.size()) return result;
    }
}
inline void pin(std::thread& thread, int cpu) {
#ifdef __linux__
    if (cpu < 0 || cpu >= CPU_SETSIZE) throw std::runtime_error("CPU inválida");
    cpu_set_t mask, actual; CPU_ZERO(&mask); CPU_SET(cpu, &mask);
    if (pthread_setaffinity_np(thread.native_handle(), sizeof(mask), &mask) ||
        pthread_getaffinity_np(thread.native_handle(), sizeof(actual), &actual) ||
        CPU_COUNT(&actual) != 1 || !CPU_ISSET(cpu, &actual))
        throw std::runtime_error("No se pudo fijar/verificar CPU " + std::to_string(cpu));
#else
    (void)thread; (void)cpu;
    throw std::runtime_error("Afinidad requiere Linux");
#endif
}
template<class... Params> void finish(const Params&... params) {
    std::cout.clear();
    if (const char* path = std::getenv("DL_PARAMS")) {
        std::ofstream file(path, std::ios::binary);
        (file.write(reinterpret_cast<const char*>(params.data()), params.size() * sizeof(double)), ...);
        if (!file) throw std::runtime_error("No se pudieron guardar los parámetros");
    }
}
}
#endif
