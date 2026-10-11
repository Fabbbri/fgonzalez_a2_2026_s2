#pragma once
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace nbody {
// Parámetros compartidos por simulación, captura y reproducción.
struct Config {
    std::size_t bodies_per_galaxy = 100;
    std::size_t steps = 10000;
    double dt = 0.002;
    std::uint32_t seed = 42;
    std::string model = "sequential";
    std::size_t quantum = 1;
    std::size_t threads = 2;
    bool no_output = false;
    std::string trajectory_path = "output/trajectory.nbody";
    std::string replay_path;
};
Config parse_arguments(int argc, char* argv[]);
void print_usage(std::ostream& output);

struct Vec2 { double x = 0.0; double y = 0.0; };
struct Body {
    double mass;
    Vec2 position;
    Vec2 velocity;
    Vec2 acceleration;
    int galaxy_id;
};
inline constexpr double gravitational_constant = 1.0;
inline constexpr double softening = 0.05;

// Un modelo distribuye el cálculo de fuerzas. Al retornar, todas las salidas
// deben estar listas: ninguna tarea puede seguir leyendo el estado anterior.
// El objeto puede mantener trabajadores/planificadores vivos entre pasos.
struct SchedulingStats {
    std::size_t tasks = 0;
    std::uint64_t rounds = 0;
    std::uint64_t dispatches = 0;
};
class ExecutionModel {
public:
    virtual ~ExecutionModel() = default;
    virtual const char* name() const = 0;
    virtual std::size_t workers() const = 0;
    virtual SchedulingStats scheduling_stats() const { return {}; }
    virtual void compute_accelerations(const std::vector<Body>& bodies,
                                      std::vector<Vec2>& accelerations,
                                      double gravity, double epsilon) = 0;
};
std::unique_ptr<ExecutionModel> make_sequential_model();
std::unique_ptr<ExecutionModel> make_fine_grained_model(std::size_t quantum);
std::unique_ptr<ExecutionModel> make_coarse_grained_model(std::size_t workers);
struct CpuTopology {
    std::size_t physical_cores = 0, logical_cpus = 0;
    std::string allowed_cpus;
    std::string smt_control = "unknown", smt_active = "unknown";
};
CpuTopology detect_cpu_topology();
void print_cpu_topology(std::ostream& output, const CpuTopology& topology);
// Cero selecciona 2*C; un valor explícito debe superar C núcleos disponibles.
std::unique_ptr<ExecutionModel> make_smt_model(std::size_t workers = 0);

// Física e integrador comunes: cada estrategia cambia la ejecución, no la entrada.
std::vector<Body> create_galaxies(std::size_t bodies_per_galaxy, std::uint32_t seed);
Vec2 acceleration_for_body(const std::vector<Body>& bodies, std::size_t index,
                           double gravity, double epsilon);
// Acumula un intervalo de fuentes en orden ascendente; permite reanudar tareas
// sin cambiar la ecuación ni reagrupar sumas respecto a la referencia.
void accumulate_accelerations(const std::vector<Body>& bodies, std::size_t index,
                              std::size_t begin, std::size_t end, Vec2& partial,
                              double gravity, double epsilon);
void refresh_accelerations(std::vector<Body>& bodies, std::vector<Vec2>& accelerations,
                           ExecutionModel& model);
void advance_one_step(std::vector<Body>& bodies, std::vector<Vec2>& accelerations,
                      double dt, ExecutionModel& model);

// Archivo v1 común a los cinco modelos; un frame en memoria a la vez.
class TrajectoryWriter {
public:
    TrajectoryWriter(const std::string& path, const Config& config, std::size_t bodies);
    void append(const std::vector<Body>& bodies);
    void finish();
private:
    std::ofstream output_;
    std::size_t count_, expected_frames_, written_frames_ = 0;
    std::vector<char> buffer_;
};
class TrajectoryReader {
public:
    explicit TrajectoryReader(const std::string& path);
    const Config& config() const { return config_; }
    std::size_t frame_count() const { return config_.steps + 1; }
    std::vector<Body> read_frame(std::size_t frame);
private:
    std::ifstream input_;
    Config config_;
    std::size_t count_ = 0;
    std::vector<char> buffer_;
};
// Reproductor independiente de las estrategias y de la región medida.
std::size_t replay_native(TrajectoryReader& trajectory, bool close_when_complete = false);
}  // namespace nbody
