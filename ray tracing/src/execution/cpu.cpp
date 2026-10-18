/**
 * @file cpu.cpp
 * Detección de topología y asignación de afinidad en Linux.
 * Las CPUs lógicas se agrupan mediante (socket, core_id), dentro de la máscara
 * permitida al hilo principal. Los identificadores de CPU no son índices de tile.
 * La afinidad limita dónde se ejecuta cada trabajador; el contador de tiles
 * sigue determinando qué región calcula.
 */
#include "execution/cpu.hpp"

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
// Comprueba que existe trabajo y que cada núcleo contiene al menos una CPU.
void validate(const CpuTopology& topology, std::size_t tiles) {
    if (tiles == 0 || topology.cores.empty()) throw std::invalid_argument("Se necesitan tiles y núcleos disponibles");
    for (const auto& core : topology.cores) if (core.empty())
        throw std::invalid_argument("Núcleo sin CPUs lógicas disponibles");
}
#ifdef __linux__
// Máscaras dinámicas: no suponer que el equipo tiene como máximo 1024 CPUs.
// Liberador RAII: CPU_ALLOC requiere CPU_FREE, no delete ni delete[].
struct FreeSet { void operator()(cpu_set_t* set) const { CPU_FREE(set); } };
// unique_ptr conserva propiedad de la máscara y la libera al salir del ámbito.
using CpuSet = std::unique_ptr<cpu_set_t, FreeSet>;
// Reserva una máscara capaz de representar bits CPUs y borra sus posiciones.
CpuSet allocate(std::size_t bits) {
    CpuSet set(CPU_ALLOC(bits));
    if (!set) throw std::bad_alloc();
    // El tamaño explícito permite operar sobre máscaras mayores que cpu_set_t fijo.
    CPU_ZERO_S(CPU_ALLOC_SIZE(bits), set.get());
    return set;
}
#endif
} // namespace
// Suma los contextos lógicos permitidos, incluidos hermanos de un núcleo.
std::size_t CpuTopology::logical_count() const {
    std::size_t count = 0;
    for (const auto& core : cores) count += core.size();
    return count;
}
CpuTopology detect_cpu_topology() {
#ifdef __linux__
    // Identificadores reales del SO que autoriza la afinidad del principal.
    std::vector<int> allowed;
    // Capacidad inicial de la máscara; crece si el kernel requiere más espacio.
    std::size_t bits = 128;
    // Reintento de lectura de afinidad con ampliación geométrica de capacidad.
    while (true) {
        auto set = allocate(bits);
        // pid = 0 consulta el hilo llamante; no modifica su afinidad.
        if (sched_getaffinity(0, CPU_ALLOC_SIZE(bits), set.get()) == 0) {
            for (std::size_t cpu = 0; cpu < bits; ++cpu)
                if (CPU_ISSET_S(cpu, CPU_ALLOC_SIZE(bits), set.get())) allowed.push_back(static_cast<int>(cpu));
            break;
        }
        // Conserva errno antes de liberar recursos o invocar otras funciones.
        const int error = errno;
        // Solo EINVAL por capacidad insuficiente permite reintentar; limita crecimiento.
        if (error != EINVAL || bits >= 1048576)
            throw std::system_error(error, std::generic_category(), "sched_getaffinity");
        bits *= 2;
    }
    // Agrupa por socket y core_id: core_id solo puede repetirse entre sockets.
    std::map<std::pair<int, int>, std::vector<int>> groups;
    // Lee la topología exclusivamente de CPUs permitidas por el entorno de ejecución.
    for (int cpu : allowed) {
        // Ruta sysfs específica de la CPU lógica; no presupone identificadores contiguos.
        const auto path = "/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/topology/";
        int package = -1, core = -1;
        std::ifstream socket_file(path + "physical_package_id"), core_file(path + "core_id");
        if (!(socket_file >> package) || !(core_file >> core) || package < 0 || core < 0)
            throw std::runtime_error("No se pudo identificar núcleo físico de CPU " + std::to_string(cpu));
        // Los identificadores con la misma pareja corresponden al mismo núcleo físico.
        groups[{package, core}].push_back(cpu);
    }
    CpuTopology topology;
    // Transfiere cada lista de hermanos a la topología, sin copiar sus elementos.
    for (auto& group : groups) topology.cores.push_back(std::move(group.second));
    // Lectura informativa del estado SMT global; no cambia BIOS ni sysfs.
    std::ifstream smt_file("/sys/devices/system/cpu/smt/active");
    std::string state;
    if (smt_file >> state && (state == "0" || state == "1")) topology.smt_active = state;
    // Exige al menos un núcleo utilizable antes de devolver la estructura.
    validate(topology, 1);
    return topology;
#else
    throw std::runtime_error("SMT/CMP con afinidad requieren Linux y su topología CPU en sysfs");
#endif
}
std::vector<int> cmp_cpus(const CpuTopology& topology, std::size_t tiles, std::size_t workers) {
    validate(topology, tiles);
    // CMP automático: P = min(M,C), un destino por núcleo.
    if (!workers) workers = std::min(tiles, topology.cores.size());
    if (workers > tiles || workers > topology.cores.size())
        throw std::invalid_argument("CMP requiere P <= núcleos físicos disponibles y P <= tiles");
    std::vector<int> cpus;
    // El primer hermano de cada núcleo aporta un destino; núcleos distintos
    // pueden tener diferente rendimiento en arquitecturas híbridas.
    for (std::size_t i = 0; i < workers; ++i) cpus.push_back(topology.cores[i].front());
    return cpus;
}
std::vector<int> smt_cpus(const CpuTopology& topology, std::size_t tiles, std::size_t workers) {
    validate(topology, tiles);
    // physical = C; logical = L. Se usa la topología permitida, no la total del equipo.
    const auto physical = topology.cores.size(), logical = topology.logical_count();
    // Sin hermanos disponibles, dos trabajadores por núcleo aproximan SMT
    // mediante sobresuscripción. Esto sigue siendo válido con SMT deshabilitado.
    if (!workers) workers = std::min(tiles, logical > physical ? logical : 2 * physical);
    if (workers > tiles) throw std::invalid_argument("SMT requiere P <= tiles");
    // Orden de destinos: primero un contexto por núcleo y luego los hermanos.
    std::vector<int> order;
    // sibling identifica la posición dentro de cada núcleo; las filas pueden
    // tener longitudes diferentes, como en una CPU con núcleos P y E.
    for (std::size_t sibling = 0; order.size() < logical; ++sibling)
        for (const auto& core : topology.cores) if (sibling < core.size()) order.push_back(core[sibling]);
    std::vector<int> cpus;
    // El módulo reutiliza destinos si P > L: sobresuscripción del sistema operativo.
    for (std::size_t i = 0; i < workers; ++i) cpus.push_back(order[i % order.size()]);
    return cpus;
}
void pin_current_thread(int cpu) {
#ifdef __linux__
    if (cpu < 0) throw std::invalid_argument("CPU negativa");
    // Capacidad mínima para representar el identificador solicitado.
    const auto bits = static_cast<std::size_t>(cpu) + 1;
    auto set = allocate(bits);
    // Activa exclusivamente el bit de la CPU destino en una máscara inicialmente vacía.
    CPU_SET_S(cpu, CPU_ALLOC_SIZE(bits), set.get());
    // Aplica la máscara al trabajador actual; pthread devuelve el error directamente.
    const int error = pthread_setaffinity_np(pthread_self(), CPU_ALLOC_SIZE(bits), set.get());
    if (error) throw std::system_error(error, std::generic_category(), "Afinidad CPU " + std::to_string(cpu));
#else
    (void)cpu;
    throw std::runtime_error("La afinidad de trabajadores requiere Linux");
#endif
}
// Serializa destinos con punto y coma para no introducir columnas en CSV.
std::string cpu_list(const std::vector<int>& cpus) {
    std::string result;
    for (int cpu : cpus) {
        if (!result.empty()) result += ';'; // No confundir con el separador del CSV.
        result += std::to_string(cpu);
    }
    return result;
}
} // namespace rt
