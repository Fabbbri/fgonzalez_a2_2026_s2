#include "interferogram/cpu_info.hpp"
#include <fstream>
#include <map>
#include <stdexcept>
#include <utility>
#ifdef __linux__
#include <sched.h>
#endif

namespace interferogram {

/*
 * Lee la topología de las CPUs que el proceso puede usar desde /sys.
 * La usan SMT y CMP para decidir en qué CPU fijar cada trabajador.
 *
 * @throws std::runtime_error si no es Linux o no se puede identificar algún núcleo.
 */
CpuInfo detectCpuInfo() {
#ifndef __linux__
    throw std::runtime_error("La detección de topología SMT requiere Linux.");
#else
    // Contamos solo las CPUs permitidas al proceso, aunque el equipo tenga más.
    cpu_set_t allowed;
    CPU_ZERO(&allowed);
    if (sched_getaffinity(0, sizeof(allowed), &allowed) != 0) {
        throw std::runtime_error("No se pudo leer la afinidad de CPU (límite CPU_SETSIZE).");
    }

    CpuInfo topology;
    // El ID de núcleo puede repetirse entre sockets; el par identifica cada núcleo.
    std::map<std::pair<int, int>, std::size_t> cores;
    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
        if (!CPU_ISSET(cpu, &allowed)) {
            continue;
        }
        const std::string directory = "/sys/devices/system/cpu/cpu" +
                                      std::to_string(cpu) + "/topology/";
        std::ifstream socketFile(directory + "physical_package_id");
        std::ifstream coreFile(directory + "core_id");
        int socket = -1;
        int core = -1;
        if (!(socketFile >> socket) || !(coreFile >> core) || socket < 0 || core < 0) {
            throw std::runtime_error("No se pudo identificar el núcleo de CPU " + std::to_string(cpu));
        }

        topology.allowedCpus.push_back(cpu);
        // Los hilos lógicos del mismo núcleo añaden el mismo par al conjunto.
        const auto inserted = cores.emplace(std::make_pair(socket, core), topology.siblingGroups.size());
        if (inserted.second) {
            topology.siblingGroups.emplace_back();
            topology.coreCpus.push_back(cpu);
        }
        topology.siblingGroups[inserted.first->second].push_back(cpu);
    }

    topology.physicalCores = cores.size();
    if (topology.physicalCores == 0) {
        throw std::runtime_error("No hay núcleos físicos disponibles para SMT.");
    }

    // Consultamos el estado SMT sin modificarlo. Si falta un archivo, queda unknown.
    std::ifstream control("/sys/devices/system/cpu/smt/control");
    std::ifstream active("/sys/devices/system/cpu/smt/active");
    std::string value;
    if (control >> value) {
        topology.smtControl = value;
    }
    if (active >> value) {
        topology.smtActive = value;
    }

    return topology;
#endif
}
}
