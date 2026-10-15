#include "nbody.hpp"
#include <limits>
#include <ostream>
#include <map>
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

// Misma política que ray tracing: primero una CPU por núcleo, luego siblings.
// La afinidad verificada permite usar SMT cuando el hardware lo tiene habilitado.
class SmtModel final : public ExecutionModel {
    std::vector<int> cpus_;
    std::unique_ptr<ExecutionModel> pool_;
public:
    explicit SmtModel(std::vector<int> cpus)
        : cpus_(std::move(cpus)), pool_(make_coarse_grained_model(cpus_.size(), cpus_, 0, 0)) {}
    const char* name() const override { return "smt"; }
    std::size_t workers() const override { return pool_->workers(); }
    SchedulingStats scheduling_stats() const override { return pool_->scheduling_stats(); }
    std::vector<int> assigned_cpus() const override { return cpus_; }
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
    std::map<std::pair<int, int>, std::size_t> cores;
    for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {
        if (!CPU_ISSET(cpu, &allowed)) continue;
        const std::string base = "/sys/devices/system/cpu/cpu" + std::to_string(cpu) + "/topology/";
        int package = -1, core = -1;
        std::ifstream package_file(base + "physical_package_id"), core_file(base + "core_id");
        if (!(package_file >> package) || !(core_file >> core) || package < 0 || core < 0) {
            throw std::runtime_error("No se pudo identificar el núcleo físico de CPU " + std::to_string(cpu));
        }
        const auto group = cores.emplace(std::make_pair(package, core), result.cores.size());
        if (group.second) {
            result.core_cpus.push_back(cpu);
            result.cores.emplace_back();
        }
        result.cores[group.first->second].push_back(cpu);
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
    output << ",sibling_groups=";
    for (std::size_t i = 0; i < topology.cores.size(); ++i) {
        if (i != 0) output << '|';
        for (std::size_t j = 0; j < topology.cores[i].size(); ++j) {
            if (j != 0) output << ';';
            output << topology.cores[i][j];
        }
    }
    if (topology.cores.empty()) output << "unknown";
    output << ",smt_control=" << topology.smt_control
           << ",smt_active=" << topology.smt_active << '\n';
}

std::vector<int> smt_cpus(const CpuTopology& topology, std::size_t workers) {
    if (topology.cores.empty()) {
        throw std::runtime_error("SMT con afinidad requiere topología de CPUs lógicas disponible en Linux.");
    }
    std::size_t logical = 0;
    for (const auto& core : topology.cores) {
        if (core.empty()) throw std::invalid_argument("Núcleo sin CPUs lógicas disponibles.");
        logical += core.size();
    }
    const auto physical = topology.cores.size();
    if (workers == 0) {
        if (physical > std::numeric_limits<std::size_t>::max() / 2) {
            throw std::invalid_argument("Demasiados núcleos para seleccionar 2*C hilos.");
        }
        workers = logical > physical ? logical : 2 * physical;
    }
    std::vector<int> order;
    for (std::size_t sibling = 0; order.size() < logical; ++sibling) {
        for (const auto& core : topology.cores) {
            if (sibling < core.size()) order.push_back(core[sibling]);
        }
    }
    std::vector<int> cpus;
    cpus.reserve(workers);
    for (std::size_t i = 0; i < workers; ++i) cpus.push_back(order[i % order.size()]);
    return cpus;
}

std::unique_ptr<ExecutionModel> make_smt_model(std::size_t workers) {
    return std::make_unique<SmtModel>(smt_cpus(detect_cpu_topology(), workers));
}
}  // namespace nbody
