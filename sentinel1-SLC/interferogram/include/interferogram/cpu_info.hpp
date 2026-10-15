#pragma once
#include <cstddef>
#include <string>
#include <vector>

namespace interferogram {

struct CpuInfo {
    // CPUs lógicas que el proceso puede usar según su afinidad actual.
    std::vector<int> allowedCpus;
    // Núcleos físicos representados por esas CPUs, sin contar hermanos SMT dos veces.
    std::size_t physicalCores = 0;
    // Una CPU lógica por núcleo físico, elegida dentro de la afinidad permitida.
    std::vector<int> coreCpus;
    // Estado informado por Linux para el equipo; unknown si no se puede leer.
    std::string smtControl = "unknown";
    std::string smtActive = "unknown";
};

// Cuenta pares (socket, núcleo) dentro de la afinidad permitida al proceso.
// Lanza una excepción si no puede identificar los núcleos disponibles.
CpuInfo detectCpuInfo();
}
