#include "nbody.hpp"
#include <limits>
#include <ostream>
#include <set>
#include <stdexcept>
#include <utility>
#if defined(__linux__)
#include <sched.h>
#elif defined(__APPLE__)
#include <sys/sysctl.h>
#endif

namespace nbody {
namespace {
#if defined(__linux__)
std::string read_state(const char* path) {
    std::ifstream input(path);
    std::string value;
    return input >> value ? value : "unknown";
}
#endif

// Reutiliza el pool y las barreras de bloques. La aproximación SMT añade una
// condición experimental obligatoria: más hilos que núcleos físicos accesibles.
// No simula instrucciones ni garantiza ejecución simultánea dentro de un núcleo.
class SmtModel final : public ExecutionModel {
    std::unique_ptr<ExecutionModel> pool_;
public:
    explicit SmtModel(std::size_t workers) : pool_(make_coarse_grained_model(workers)) {}
    const char* name() const override { return "smt"; }
    std::size_t workers() const override { return pool_->workers(); }
    SchedulingStats scheduling_stats() const override { return pool_->scheduling_stats(); }
    void compute_accelerations(const std::vector<Body>& bodies,
                               std::vector<Vec2>& accelerations,
                               double gravity, double epsilon) override {
        pool_->compute_accelerations(bodies, accelerations, gravity, epsilon);
    }
};
}  // namespace

CpuTopology detect_cpu_topology() {
    CpuTopology result;
#if defined(__linux__)
    cpu_set_t allowed;
    CPU_ZERO(&allowed);
    if (sched_getaffinity(0, sizeof(allowed), &allowed) != 0) {
        throw std::runtime_error("No se pudo leer la afinidad de CPU (o excede CPU_SETSIZE).");
    }
    std::set<std::pair<int, int>> cores;
    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
        if (!CPU_ISSET(cpu, &allowed)) continue;
        const std::string base = "/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/topology/";
        int package = -1, core = -1;
        std::ifstream package_file(base + "physical_package_id"), core_file(base + "core_id");
        if (!(package_file >> package) || !(core_file >> core) || package < 0 || core < 0) {
            throw std::runtime_error("No se pudo identificar el núcleo físico de CPU " + std::to_string(cpu));
        }
        if (cores.emplace(package, core).second) result.core_cpus.push_back(cpu);
        if (!result.allowed_cpus.empty()) result.allowed_cpus += ';';
        result.allowed_cpus += std::to_string(cpu);
        ++result.logical_cpus;
    }
    result.physical_cores = cores.size();
    result.smt_control = read_state("/sys/devices/system/cpu/smt/control");
    result.smt_active = read_state("/sys/devices/system/cpu/smt/active");
#elif defined(__APPLE__)
    // macOS no expone una máscara de afinidad equivalente: se usa el sistema.
    unsigned int physical = 0, logical = 0;
    std::size_t size = sizeof(physical);
    if (sysctlbyname("hw.physicalcpu", &physical, &size, nullptr, 0) != 0 ||
        sysctlbyname("hw.logicalcpu", &logical, &size, nullptr, 0) != 0) {
        throw std::runtime_error("No se pudo leer la topología de CPU de macOS.");
    }
    result.physical_cores = physical;
    result.logical_cpus = logical;
    result.allowed_cpus = "system";
    // Una razón 1:1 no permite distinguir desactivado de no soportado.
    if (logical > physical) result.smt_active = "1";
#else
    throw std::runtime_error("Detección de núcleos físicos disponible solo en Linux y macOS.");
#endif
    if (result.physical_cores == 0 || result.logical_cpus < result.physical_cores) {
        throw std::runtime_error("Topología de CPU inválida.");
    }
    return result;
}

void print_cpu_topology(std::ostream& output, const CpuTopology& topology) {
    output << "physical_cores=" << topology.physical_cores
           << ",logical_cpus=" << topology.logical_cpus
           << ",allowed_cpus=" << topology.allowed_cpus
           << ",core_cpus=";
    for (std::size_t i = 0; i < topology.core_cpus.size(); ++i) {
        if (i != 0) output << ';';
        output << topology.core_cpus[i];
    }
    if (topology.core_cpus.empty()) output << "unknown";
    output << ",smt_control=" << topology.smt_control
           << ",smt_active=" << topology.smt_active << '\n';
}

std::unique_ptr<ExecutionModel> make_smt_model(std::size_t workers) {
    const auto topology = detect_cpu_topology();
    if (workers == 0) {
        if (topology.physical_cores > std::numeric_limits<std::size_t>::max() / 2) {
            throw std::invalid_argument("Demasiados núcleos para seleccionar 2*C hilos.");
        }
        workers = 2 * topology.physical_cores;
    }
    if (workers <= topology.physical_cores) {
        throw std::invalid_argument("SMT requiere --threads mayor que los núcleos físicos disponibles (" +
                                    std::to_string(topology.physical_cores) + ").");
    }
    return std::make_unique<SmtModel>(workers);
}
}  // namespace nbody
