/**
 * @file nbody.hpp
 * Contrato del ejecutable multihilo construido por el Makefile.
 * Agrupa configuración, estado físico, estrategias de ejecución, topología CPU
 * y formato de trayectoria. Las implementaciones viven en common/ y models/.
 *
 * El dominio paralelo es el índice i del cuerpo receptor de aceleración.
 * Los modelos leen una misma instantánea bodies y producen accelerations[i].
 * El retorno de compute_accelerations representa la finalización de la fase:
 * el integrador solo modifica posiciones cuando ningún trabajador las sigue leyendo.
 */
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
// Configuración de una ejecución o reproducción; los valores iniciales definen la carga por defecto.
struct Config {
    // Población por disco; el total de receptores y fuentes es N=2*bodies_per_galaxy.
    std::size_t bodies_per_galaxy = 500;
    // Pasos físicos solicitados; la trayectoria contiene steps+1 frames, incluido el inicial.
    std::size_t steps = 1000;
    // Incremento temporal positivo del integrador, independiente de la velocidad visual.
    double dt = 0.002;
    // Semilla de 32 bits para reproducir la distribución inicial.
    std::uint32_t seed = 42;
    // Nombre de estrategia que determina cómo se ejecuta cada fase de aceleraciones.
    std::string model = "sequential";
    // K interacciones por turno de una tarea fine, no milisegundos.
    std::size_t quantum = 1;
    // P solicitado para el grupo; el analizador convierte omisión en automático para SMT/CMP.
    std::size_t threads = 2;
    // Tiempo de pared entre pausas simuladas de coarse; cero las desactiva.
    int block_interval_ms = 10;
    // Duración de suspensión pedida por bloqueo, en milisegundos.
    int block_duration_ms = 1;
    // Omite ventana al terminar; conserva archivo de trayectoria y métricas.
    bool no_output = false;
    // Destino binario de todos los estados calculados.
    std::string trajectory_path = "output/trajectory.nbody";
    // Ruta de reproducción; una cadena vacía indica simulación nueva.
    std::string replay_path;
};
// Interpreta argc/argv y rechaza combinaciones incompatibles antes de crear modelos.
Config parse_arguments(int argc, char* argv[]);
// Escribe ayuda textual en el stream proporcionado, sin iniciar simulación.
void print_usage(std::ostream& output);

// Vector cartesiano plano inicializado a cero; se usa para posición, velocidad y aceleración.
struct Vec2 { double x = 0.0; double y = 0.0; };
// Estado de una partícula. El índice del vector identifica el receptor paralelo i.
struct Body {
    // Masa positiva constante; solo m_j pondera la aceleración que la fuente j produce.
    double mass;
    // Posición r_i de la instantánea; permanece inmutable durante una fase de fuerzas.
    Vec2 position;
    // Velocidad v_i actualizada por el integrador en el coordinador.
    Vec2 velocity;
    // a_i de la última fase completa, usada para el siguiente paso.
    Vec2 acceleration;
    // Origen 0 o 1 para presentación; no limita las fuentes de atracción.
    int galaxy_id;
};
// Constante gravitacional G=1 en unidades normalizadas, compartida por todos los modelos.
inline constexpr double gravitational_constant = 1.0;
// Longitud epsilon que regulariza el denominador de fuerzas cercanas.
inline constexpr double softening = 0.05;

// Un modelo distribuye el cálculo de fuerzas. Al retornar, todas las salidas
// deben estar listas: ninguna tarea puede seguir leyendo el estado anterior.
// El objeto puede mantener trabajadores/planificadores vivos entre pasos.
// Contadores lógicos acumulados; no representan directamente eventos de hardware.
struct SchedulingStats {
    // Fine: N tareas de cuerpos. Grupo persistente: P particiones.
    std::size_t tasks = 0;
    // Fine: rondas completas; grupo: fases completadas de cálculo de aceleraciones.
    std::uint64_t rounds = 0;
    // Turnos de tareas fine o activaciones de trabajadores por fase.
    std::uint64_t dispatches = 0;
    // Esperas temporizadas de coarse acumuladas durante las fases.
    std::uint64_t blocks = 0;  // Esperas temporales coarse, acumuladas entre fases.
};
// Interfaz polimórfica de una fase; el destructor virtual permite liberar grupos mediante la base.
class ExecutionModel {
public:
    // Destrucción dinámica correcta al liberar unique_ptr<ExecutionModel>.
    virtual ~ExecutionModel() = default;
    // Identidad textual de la estrategia, sin modificar estado.
    virtual const char* name() const = 0;
    // Número real de ejecutores; fine conserva P=1 aunque tenga N contextos lógicos.
    virtual std::size_t workers() const = 0;
    // Estadísticas opcionales; la base devuelve ceros si el modelo no las redefine.
    virtual SchedulingStats scheduling_stats() const { return {}; }
    // Destinos explícitos opcionales; la base devuelve vacío.
    virtual std::vector<int> assigned_cpus() const { return {}; }
    // Contrato de fase: entrada solo lectura y una salida Vec2 por receptor i.
    // Al retornar todos los trabajadores han terminado; el llamante puede modificar cuerpos.
    // Las llamadas no son concurrentes entre sí: el integrador coordina fases sucesivas.
    virtual void compute_accelerations(const std::vector<Body>& bodies,
                                      std::vector<Vec2>& accelerations,
                                      double gravity, double epsilon) = 0;
};
// Fábrica del recorrido exterior i con un único ejecutor.
std::unique_ptr<ExecutionModel> make_sequential_model();
// Fábrica cooperativa con quantum positivo y parciales privados por cuerpo.
std::unique_ptr<ExecutionModel> make_fine_grained_model(std::size_t quantum);
// Pool compartido por Coarse/SMT/CMP. CPU opcionales: una por trabajador en Linux.
// Fábrica de grupo persistente; cpus vacío no fija afinidad y bloqueos cero la desactivan.
std::unique_ptr<ExecutionModel> make_coarse_grained_model(
    std::size_t workers, const std::vector<int>& cpus = {},
    int block_interval_ms = 10, int block_duration_ms = 1);
// Recursos visibles al proceso; C y L pueden ser menores que los totales físicos del equipo.
struct CpuTopology {
    // C núcleos físicos y L contextos lógicos permitidos por el sistema.
    std::size_t physical_cores = 0, logical_cpus = 0;
    // Lista serializada de IDs lógicos, separados por punto y coma en Linux.
    std::string allowed_cpus;
    // Estados publicados por sysfs; unknown conserva ausencia de información.
    std::string smt_control = "unknown", smt_active = "unknown";
    // Representante lógico de cada núcleo, usado para asignación CMP.
    std::vector<int> core_cpus;  // Una CPU lógica permitida por núcleo físico (Linux).
    // Grupos físicos de hermanos, de longitudes potencialmente diferentes.
    std::vector<std::vector<int>> cores;  // CPUs permitidas agrupadas por socket y núcleo.
};
// Consulta afinidad y sysfs en Linux; cantidades de sysctl en macOS.
CpuTopology detect_cpu_topology();
// Emite formato estructurado para scripts y diagnóstico.
void print_cpu_topology(std::ostream& output, const CpuTopology& topology);
// Primero una CPU por núcleo, después sus siblings; si P > L reutiliza CPUs.
// Asigna por rondas de hermanos; workers=0 selecciona L o 2*C.
std::vector<int> smt_cpus(const CpuTopology& topology, std::size_t workers = 0);
// Cero selecciona L si hay siblings disponibles, o 2*C por sobresuscripción.
// Grupo de afinidad lógica sin bloqueos temporizados de coarse.
std::unique_ptr<ExecutionModel> make_smt_model(std::size_t workers = 0);
// Cero selecciona todos los núcleos físicos disponibles; no sobresuscribe.
// Grupo con como máximo un destino por núcleo físico disponible.
std::unique_ptr<ExecutionModel> make_cmp_model(std::size_t workers = 0);

// Física e integrador comunes: cada estrategia cambia la ejecución, no la entrada.
// Genera los dos discos de forma determinista con la semilla recibida.
std::vector<Body> create_galaxies(std::size_t bodies_per_galaxy, std::uint32_t seed);
// Kernel completo del receptor index; retorna un acumulado privado sobre todas las fuentes.
Vec2 acceleration_for_body(const std::vector<Body>& bodies, std::size_t index,
                           double gravity, double epsilon);
// Acumula un intervalo de fuentes en orden ascendente; permite reanudar tareas
// sin cambiar la ecuación ni reagrupar sumas respecto a la referencia.
// Suma fuentes [begin,end) en orden ascendente sobre partial del receptor index.
// El llamante conserva propiedad exclusiva de ese parcial.
void accumulate_accelerations(const std::vector<Body>& bodies, std::size_t index,
                              std::size_t begin, std::size_t end, Vec2& partial,
                              double gravity, double epsilon);
// Ejecuta la barrera de fase, valida resultados y copia aceleraciones al estado Body.
void refresh_accelerations(std::vector<Body>& bodies, std::vector<Vec2>& accelerations,
                           ExecutionModel& model);
// Actualiza v, luego r, y calcula la aceleración del nuevo estado con el mismo modelo.
void advance_one_step(std::vector<Body>& bodies, std::vector<Vec2>& accelerations,
                      double dt, ExecutionModel& model);

// Archivo v1 común a los cinco modelos; un frame en memoria a la vez.
// Escritor de frames completos; no comparte archivo con los trabajadores de fuerzas.
class TrajectoryWriter {
public:
    // Valida dimensiones, crea carpeta y escribe cabecera; reserva un buffer de frame.
    TrajectoryWriter(const std::string& path, const Config& config, std::size_t bodies);
    // Serializa el estado después de una fase completa, sin capturar posiciones intermedias.
    void append(const std::vector<Body>& bodies);
    // Exige steps+1 frames y comprueba vaciado y cierre del archivo.
    void finish();
private:
    // Stream binario propietario del destino.
    std::ofstream output_;
    // Cuerpos por frame, frames esperados y contador de frames escritos.
    std::size_t count_, expected_frames_, written_frames_ = 0;
    // Buffer de bytes de un frame reutilizado en cada escritura.
    std::vector<char> buffer_;
};
// Lector por acceso aleatorio; valida cabecera, tamaño y representación.
class TrajectoryReader {
public:
    // Abre un archivo de versión y orden de bytes compatibles.
    explicit TrajectoryReader(const std::string& path);
    // Vista inmutable de la configuración reconstruida desde la cabecera.
    const Config& config() const { return config_; }
    // Cuenta estado inicial más todos los pasos guardados.
    std::size_t frame_count() const { return config_.steps + 1; }
    // Recupera un frame validado a partir de su índice, sin integrar física.
    std::vector<Body> read_frame(std::size_t frame);
private:
    // Stream binario del archivo fuente, usado solo por el reproductor.
    std::ifstream input_;
    // Parámetros reconstruidos: cuerpos por galaxia, pasos, dt y semilla.
    Config config_;
    // Número total de cuerpos de cada frame del archivo.
    std::size_t count_ = 0;
    std::vector<char> buffer_;
};
// Reproductor independiente de las estrategias y de la región medida.
// Presenta frames SDL y devuelve el último índice; cierre automático se usa en ensayos.
std::size_t replay_native(TrajectoryReader& trajectory, bool close_when_complete = false);
}  // namespace nbody
