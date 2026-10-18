/**
 * @file cpu.hpp
 * Interfaz de topología CPU, selección de destinos y afinidad de trabajadores.
 * C = núcleos físicos disponibles; L = CPUs lógicas; P = trabajadores; M = tiles.
 */
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace rt {
// Vista de recursos permitidos al proceso, que puede ser menor que el equipo físico.
struct CpuTopology {
    // Cada fila agrupa CPUs lógicas del mismo (socket, núcleo físico).
    // Solo se incluyen CPUs permitidas por la afinidad del hilo principal.
    std::vector<std::vector<int>> cores;
    std::string smt_active = "desconocido"; // Estado informado por Linux, no modifica BIOS.
    // Cuenta L sumando las longitudes de todas las listas de hermanos.
    std::size_t logical_count() const;
};
// Consulta máscara de afinidad, socket, núcleo y estado SMT en Linux.
CpuTopology detect_cpu_topology();
// Un CPU lógico por núcleo físico; P no puede superar C ni M tiles.
// Devuelve P destinos, uno por núcleo; workers=0 elige min(C,M).
std::vector<int> cmp_cpus(const CpuTopology& topology, std::size_t tiles, std::size_t workers = 0);
// Primero un CPU por núcleo, luego sus hermanos SMT. P puede superar C y L.
// Si P > L, se reutilizan CPUs: sobresuscripción del SO, no más hardware SMT.
// Devuelve P destinos siguiendo hermanos por rondas; P>L reutiliza destinos.
std::vector<int> smt_cpus(const CpuTopology& topology, std::size_t tiles, std::size_t workers = 0);
// Restringe el hilo llamante a una sola CPU lógica y propaga errores de afinidad.
void pin_current_thread(int cpu);
// Representación de destinos separada por punto y coma para incluirla en CSV.
std::string cpu_list(const std::vector<int>& cpus);
} // namespace rt
