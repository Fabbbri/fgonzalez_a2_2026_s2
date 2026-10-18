/**
 * @file trajectory.cpp
 * Serialización binaria de estados para reproducción separada del cálculo.
 * La cabecera v1 ocupa 40 bytes. Cada cuerpo almacena siete double y una
 * galaxia int32, sin depender del padding de Body. El lector exige el mismo
 * orden de bytes y representación IEEE 754, y conserva un frame de bytes por vez.
 */
#include "nbody.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>
#include <stdexcept>

namespace nbody {
namespace {
// Firma de ocho bytes: identifica formato NBODY001 y su versión.
constexpr std::array<char, 8> magic{{'N', 'B', 'O', 'D', 'Y', '0', '0', '1'}};
// Marcador que detecta diferencias de endianidad; el lector rechaza formatos incompatibles.
constexpr std::uint32_t byte_order = 0x01020304;
// Tamaño fijo: 8 firma + 4 orden + 8 cuerpos + 8 pasos + 8 dt + 4 semilla.
constexpr std::uint64_t header_bytes = 40;
// Registro explícito de 60 bytes: siete doubles IEEE754 y un entero de galaxia.
constexpr std::uint64_t body_bytes = 7 * sizeof(double) + sizeof(std::int32_t);
// Garantía de representación en compilación antes de escribir bytes crudos de double.
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559,
              "El formato requiere double IEEE 754 de 64 bits.");

// Serializa un escalar por representación binaria, no por texto ni padding de structs.
template<class T> void write_value(std::ostream& out, T value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}
// Lee exactamente sizeof(T) bytes; el llamante comprueba estado del stream.
template<class T> T read_value(std::istream& in) {
    T value{};
    in.read(reinterpret_cast<char*>(&value), sizeof(value));
    return value;
}
// Se comprueban los productos antes de reservar memoria o calcular offsets.
// Valida cantidades y productos antes de reservar un buffer o calcular desplazamientos.
std::size_t frame_bytes(std::uint64_t count, std::uint64_t steps) {
    // Límite representable de streamoff para acceso aleatorio en el archivo.
    const auto limit = static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max());
    // N positivo y par representa las dos galaxias; verifica tamaños de memoria y archivo.
    if (count == 0 || count % 2 != 0 || steps >= std::numeric_limits<std::size_t>::max() ||
        count > std::vector<Body>{}.max_size() || count > limit / body_bytes ||
        count > std::numeric_limits<std::size_t>::max() / body_bytes) {
        throw std::runtime_error("Dimensiones de trayectoria no válidas.");
    }
    const auto bytes = count * body_bytes;
    // Comprueba el tamaño total por división antes de multiplicar frames por bytes de frame.
    if (steps + 1 > (limit - header_bytes) / bytes) {
        throw std::runtime_error("La trayectoria excede el tamaño de archivo admitido.");
    }
    return static_cast<std::size_t>(bytes);
}
}  // namespace

// Reserva un frame, crea carpetas y abre archivo truncando un destino previo.
TrajectoryWriter::TrajectoryWriter(const std::string& path, const Config& config,
                                   std::size_t bodies)
    : count_(bodies), expected_frames_(config.steps + 1),
      buffer_(frame_bytes(bodies, config.steps)) {
    // Una ruta simple no necesita crear carpetas; una ruta compuesta sí prepara su padre.
    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    output_.open(path, std::ios::binary | std::ios::trunc);
    if (!output_) throw std::runtime_error("No se pudo crear la trayectoria: " + path);
    // La cabecera precede todos los frames y almacena la configuración física básica.
    output_.write(magic.data(), magic.size());
    write_value(output_, byte_order);
    write_value(output_, static_cast<std::uint64_t>(bodies));
    write_value(output_, static_cast<std::uint64_t>(config.steps));
    write_value(output_, config.dt);
    write_value(output_, config.seed);
    if (!output_) throw std::runtime_error("No se pudo escribir la cabecera.");
}

// Agrupa un frame completo en el buffer reutilizable y realiza una escritura contigua.
void TrajectoryWriter::append(const std::vector<Body>& bodies) {
    // Exige archivo activo, mismo N y capacidad restante de frames esperados.
    if (!output_.is_open() || bodies.size() != count_ || written_frames_ >= expected_frames_) {
        throw std::runtime_error("Estado inesperado al guardar la trayectoria.");
    }
    // Puntero de empaquetado que avanza sin realocar el buffer.
    char* cursor = buffer_.data();
    // El orden del vector conserva identidad i de cada partícula entre frames.
    for (const Body& body : bodies) {
        // Orden explícito: masa, x,y, vx,vy, ax,ay; no serializa memoria de Body directamente.
        const double values[]{body.mass, body.position.x, body.position.y,
            body.velocity.x, body.velocity.y, body.acceleration.x, body.acceleration.y};
        // Fija el ancho de galaxia en 32 bits para que el registro no dependa de sizeof(int).
        const auto galaxy = static_cast<std::int32_t>(body.galaxy_id);
        // Copia escalares al buffer y avanza el cursor por su tamaño exacto.
        std::memcpy(cursor, values, sizeof(values)); cursor += sizeof(values);
        std::memcpy(cursor, &galaxy, sizeof(galaxy)); cursor += sizeof(galaxy);
    }
    // Escritura de un frame después de reunir todos los campos.
    output_.write(buffer_.data(), static_cast<std::streamsize>(buffer_.size()));
    if (!output_) throw std::runtime_error("Error al escribir un estado de la trayectoria.");
    // Solo cuenta un frame cuando el stream confirmó una escritura válida.
    ++written_frames_;
}

// Rechaza captura incompleta y comprueba también fallos tardíos de flush o close.
void TrajectoryWriter::finish() {
    if (!output_.is_open() || written_frames_ != expected_frames_) {
        throw std::runtime_error("Trayectoria incompleta.");
    }
    output_.flush();
    output_.close();
    if (!output_) throw std::runtime_error("Error al finalizar la trayectoria.");
}

// Verifica firma, parámetros y tamaño total antes de permitir lectura aleatoria.
TrajectoryReader::TrajectoryReader(const std::string& path) {
    input_.open(path, std::ios::binary);
    if (!input_) throw std::runtime_error("No se pudo abrir la trayectoria: " + path);
    std::array<char, 8> signature{};
    input_.read(signature.data(), signature.size());
    const auto order = read_value<std::uint32_t>(input_);
    if (!input_ || signature != magic || order != byte_order) {
        throw std::runtime_error("Trayectoria incompatible: formato, versión u orden de bytes.");
    }
    // Reconstruye cantidades de 64 bits antes de convertirlas al tamaño nativo validado.
    const auto count = read_value<std::uint64_t>(input_);
    const auto steps = read_value<std::uint64_t>(input_);
    config_.dt = read_value<double>(input_);
    config_.seed = read_value<std::uint32_t>(input_);
    if (!input_ || !std::isfinite(config_.dt) || config_.dt <= 0 ||
        !std::isfinite(config_.dt * static_cast<double>(steps))) {
        throw std::runtime_error("Cabecera de trayectoria no válida.");
    }
    const auto bytes = frame_bytes(count, steps);
    // Tamaño exacto de cabecera más N registros por cada uno de steps+1 frames.
    const auto expected = header_bytes + (steps + 1) * bytes;
    // Desplaza al final para rechazar truncamiento y datos adicionales.
    input_.seekg(0, std::ios::end);
    if (!input_ || input_.tellg() != static_cast<std::streamoff>(expected)) {
        throw std::runtime_error("Trayectoria truncada o con datos adicionales.");
    }
    count_ = static_cast<std::size_t>(count);
    // N par se divide entre dos para recuperar la configuración de generación.
    config_.bodies_per_galaxy = count_ / 2;
    config_.steps = static_cast<std::size_t>(steps);
    buffer_.resize(bytes);
}

// Acceso por índice frame: no requiere leer todos los frames anteriores.
std::vector<Body> TrajectoryReader::read_frame(std::size_t frame) {
    if (frame >= frame_count()) throw std::out_of_range("Frame fuera de la trayectoria.");
    // Desplazamiento = cabecera + frame*tamaño_frame; los productos ya fueron validados.
    input_.seekg(static_cast<std::streamoff>(header_bytes + frame * buffer_.size()));
    input_.read(buffer_.data(), static_cast<std::streamsize>(buffer_.size()));
    if (!input_) throw std::runtime_error("No se pudo leer el frame.");
    // Reconstruye solo el estado solicitado, no la trayectoria completa en memoria.
    std::vector<Body> bodies(count_);
    // Puntero de lectura del buffer, simétrico al empaquetado del escritor.
    const char* cursor = buffer_.data();
    for (Body& body : bodies) {
        double values[7];
        std::int32_t galaxy;
        std::memcpy(values, cursor, sizeof(values)); cursor += sizeof(values);
        std::memcpy(&galaxy, cursor, sizeof(galaxy)); cursor += sizeof(galaxy);
        // Comprueba finitud de masa, posición, velocidad y aceleración.
        for (double value : values) {
            if (!std::isfinite(value)) throw std::runtime_error("Estado no finito en la trayectoria.");
        }
        // Valida masa positiva e identidad de una de las dos galaxias.
        if (values[0] <= 0 || (galaxy != 0 && galaxy != 1)) {
            throw std::runtime_error("Masa o galaxia no válida en la trayectoria.");
        }
        // Reconstituye la estructura de estado a partir de los campos en orden fijo.
        body = {values[0], {values[1], values[2]}, {values[3], values[4]},
                {values[5], values[6]}, galaxy};
    }
    return bodies;
}
}  // namespace nbody
