#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace rt {
struct CpuTopology {
    // Cada fila agrupa CPUs lógicas del mismo (socket, núcleo físico).
    // Solo se incluyen CPUs permitidas por la afinidad del hilo principal.
    std::vector<std::vector<int>> cores;
    std::string smt_active = "desconocido"; // Estado informado por Linux, no modifica BIOS.
    std::size_t logical_count() const;
};
CpuTopology detect_cpu_topology();
// Un CPU lógico por núcleo físico; P no puede superar C ni M tiles.
std::vector<int> cmp_cpus(const CpuTopology& topology, std::size_t tiles, std::size_t workers = 0);
// Primero un CPU por núcleo, luego sus hermanos SMT. P puede superar C y L.
// Si P > L, se reutilizan CPUs: sobresuscripción del SO, no más hardware SMT.
std::vector<int> smt_cpus(const CpuTopology& topology, std::size_t tiles, std::size_t workers = 0);
void pin_current_thread(int cpu);
std::string cpu_list(const std::vector<int>& cpus);
} // namespace rt
