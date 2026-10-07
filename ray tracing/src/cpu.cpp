#include "cpu.hpp"

#include <algorithm>
#include <cerrno>
#include <fstream>
#include <map>
#include <memory>
#include <stdexcept>
#include <system_error>
#ifdef __linux__
#include <pthread.h>
#include <sched.h>
#endif

namespace rt {
namespace {
void validate(const CpuTopology& topology, std::size_t tiles) {
    if (tiles == 0 || topology.cores.empty()) throw std::invalid_argument("Se necesitan tiles y núcleos disponibles");
    for (const auto& core : topology.cores) if (core.empty())
        throw std::invalid_argument("Núcleo sin CPUs lógicas disponibles");
}
#ifdef __linux__
// Máscaras dinámicas: no suponer que el equipo tiene como máximo 1024 CPUs.
struct FreeSet { void operator()(cpu_set_t* set) const { CPU_FREE(set); } };
using CpuSet = std::unique_ptr<cpu_set_t, FreeSet>;
CpuSet allocate(std::size_t bits) {
    CpuSet set(CPU_ALLOC(bits));
    if (!set) throw std::bad_alloc();
    CPU_ZERO_S(CPU_ALLOC_SIZE(bits), set.get());
    return set;
}
#endif
} // namespace
std::size_t CpuTopology::logical_count() const {
    std::size_t count = 0;
    for (const auto& core : cores) count += core.size();
    return count;
}
CpuTopology detect_cpu_topology() {
#ifdef __linux__
    std::vector<int> allowed;
    std::size_t bits = 128;
    while (true) {
        auto set = allocate(bits);
        if (sched_getaffinity(0, CPU_ALLOC_SIZE(bits), set.get()) == 0) {
            for (std::size_t cpu = 0; cpu < bits; ++cpu)
                if (CPU_ISSET_S(cpu, CPU_ALLOC_SIZE(bits), set.get())) allowed.push_back(static_cast<int>(cpu));
            break;
        }
        const int error = errno;
        if (error != EINVAL || bits >= 1048576)
            throw std::system_error(error, std::generic_category(), "sched_getaffinity");
        bits *= 2;
    }
    std::map<std::pair<int, int>, std::vector<int>> groups;
    for (int cpu : allowed) {
        const auto path = "/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/topology/";
        int package = -1, core = -1;
        std::ifstream socket_file(path + "physical_package_id"), core_file(path + "core_id");
        if (!(socket_file >> package) || !(core_file >> core) || package < 0 || core < 0)
            throw std::runtime_error("No se pudo identificar núcleo físico de CPU " + std::to_string(cpu));
        groups[{package, core}].push_back(cpu);
    }
    CpuTopology topology;
    for (auto& group : groups) topology.cores.push_back(std::move(group.second));
    std::ifstream smt_file("/sys/devices/system/cpu/smt/active");
    std::string state;
    if (smt_file >> state && (state == "0" || state == "1")) topology.smt_active = state;
    validate(topology, 1);
    return topology;
#else
    throw std::runtime_error("SMT/CMP con afinidad requieren Linux y su topología CPU en sysfs");
#endif
}
std::vector<int> cmp_cpus(const CpuTopology& topology, std::size_t tiles, std::size_t workers) {
    validate(topology, tiles);
    if (!workers) workers = std::min(tiles, topology.cores.size());
    if (workers > tiles || workers > topology.cores.size())
        throw std::invalid_argument("CMP requiere P <= núcleos físicos disponibles y P <= tiles");
    std::vector<int> cpus;
    for (std::size_t i = 0; i < workers; ++i) cpus.push_back(topology.cores[i].front());
    return cpus;
}
std::vector<int> smt_cpus(const CpuTopology& topology, std::size_t tiles, std::size_t workers) {
    validate(topology, tiles);
    const auto physical = topology.cores.size(), logical = topology.logical_count();
    // Sin hermanos disponibles, dos trabajadores por núcleo aproximan SMT
    // mediante sobresuscripción. Esto sigue siendo válido con SMT deshabilitado.
    if (!workers) workers = std::min(tiles, logical > physical ? logical : 2 * physical);
    if (workers > tiles) throw std::invalid_argument("SMT requiere P <= tiles");
    std::vector<int> order;
    for (std::size_t sibling = 0; order.size() < logical; ++sibling)
        for (const auto& core : topology.cores) if (sibling < core.size()) order.push_back(core[sibling]);
    std::vector<int> cpus;
    for (std::size_t i = 0; i < workers; ++i) cpus.push_back(order[i % order.size()]);
    return cpus;
}
void pin_current_thread(int cpu) {
#ifdef __linux__
    if (cpu < 0) throw std::invalid_argument("CPU negativa");
    const auto bits = static_cast<std::size_t>(cpu) + 1;
    auto set = allocate(bits);
    CPU_SET_S(cpu, CPU_ALLOC_SIZE(bits), set.get());
    const int error = pthread_setaffinity_np(pthread_self(), CPU_ALLOC_SIZE(bits), set.get());
    if (error) throw std::system_error(error, std::generic_category(), "Afinidad CPU " + std::to_string(cpu));
#else
    (void)cpu;
    throw std::runtime_error("La afinidad de trabajadores requiere Linux");
#endif
}
std::string cpu_list(const std::vector<int>& cpus) {
    std::string result;
    for (int cpu : cpus) {
        if (!result.empty()) result += ';'; // No confundir con el separador del CSV.
        result += std::to_string(cpu);
    }
    return result;
}
} // namespace rt
