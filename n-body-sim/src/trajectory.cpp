#include "nbody/trajectory.hpp"
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>
#include <stdexcept>

namespace nbody {
namespace {
constexpr std::array<char, 8> magic{{'N', 'B', 'O', 'D', 'Y', '0', '0', '1'}};
constexpr std::uint32_t byte_order = 0x01020304;
constexpr std::uint64_t header_bytes = 40;
constexpr std::uint64_t body_bytes = 7 * sizeof(double) + sizeof(std::int32_t);
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559,
              "El formato requiere double IEEE 754 de 64 bits.");

template<class T> void write_value(std::ostream& out, T value) {
    out.write(reinterpret_cast<const char*>(&value), sizeof(value));
}
template<class T> T read_value(std::istream& in) {
    T value{};
    in.read(reinterpret_cast<char*>(&value), sizeof(value));
    return value;
}
// Se comprueban los productos antes de reservar memoria o calcular offsets.
std::size_t frame_bytes(std::uint64_t count, std::uint64_t steps) {
    const auto limit = static_cast<std::uint64_t>(std::numeric_limits<std::streamoff>::max());
    if (count == 0 || count % 2 != 0 || steps >= std::numeric_limits<std::size_t>::max() ||
        count > std::vector<Body>{}.max_size() || count > limit / body_bytes ||
        count > std::numeric_limits<std::size_t>::max() / body_bytes) {
        throw std::runtime_error("Dimensiones de trayectoria no válidas.");
    }
    const auto bytes = count * body_bytes;
    if (steps + 1 > (limit - header_bytes) / bytes) {
        throw std::runtime_error("La trayectoria excede el tamaño de archivo admitido.");
    }
    return static_cast<std::size_t>(bytes);
}
}  // namespace

TrajectoryWriter::TrajectoryWriter(const std::string& path, const Config& config,
                                   std::size_t bodies)
    : count_(bodies), expected_frames_(config.steps + 1),
      buffer_(frame_bytes(bodies, config.steps)) {
    const auto parent = std::filesystem::path(path).parent_path();
    if (!parent.empty()) std::filesystem::create_directories(parent);
    output_.open(path, std::ios::binary | std::ios::trunc);
    if (!output_) throw std::runtime_error("No se pudo crear la trayectoria: " + path);
    output_.write(magic.data(), magic.size());
    write_value(output_, byte_order);
    write_value(output_, static_cast<std::uint64_t>(bodies));
    write_value(output_, static_cast<std::uint64_t>(config.steps));
    write_value(output_, config.dt);
    write_value(output_, config.seed);
    if (!output_) throw std::runtime_error("No se pudo escribir la cabecera.");
}

void TrajectoryWriter::append(const std::vector<Body>& bodies) {
    if (!output_.is_open() || bodies.size() != count_ || written_frames_ >= expected_frames_) {
        throw std::runtime_error("Estado inesperado al guardar la trayectoria.");
    }
    char* cursor = buffer_.data();
    for (const Body& body : bodies) {
        const double values[]{body.mass, body.position.x, body.position.y,
            body.velocity.x, body.velocity.y, body.acceleration.x, body.acceleration.y};
        const auto galaxy = static_cast<std::int32_t>(body.galaxy_id);
        std::memcpy(cursor, values, sizeof(values)); cursor += sizeof(values);
        std::memcpy(cursor, &galaxy, sizeof(galaxy)); cursor += sizeof(galaxy);
    }
    output_.write(buffer_.data(), static_cast<std::streamsize>(buffer_.size()));
    if (!output_) throw std::runtime_error("Error al escribir un estado de la trayectoria.");
    ++written_frames_;
}

void TrajectoryWriter::finish() {
    if (!output_.is_open() || written_frames_ != expected_frames_) {
        throw std::runtime_error("Trayectoria incompleta.");
    }
    output_.flush();
    output_.close();
    if (!output_) throw std::runtime_error("Error al finalizar la trayectoria.");
}

TrajectoryReader::TrajectoryReader(const std::string& path) {
    input_.open(path, std::ios::binary);
    if (!input_) throw std::runtime_error("No se pudo abrir la trayectoria: " + path);
    std::array<char, 8> signature{};
    input_.read(signature.data(), signature.size());
    const auto order = read_value<std::uint32_t>(input_);
    if (!input_ || signature != magic || order != byte_order) {
        throw std::runtime_error("Trayectoria incompatible: formato, versión u orden de bytes.");
    }
    const auto count = read_value<std::uint64_t>(input_);
    const auto steps = read_value<std::uint64_t>(input_);
    config_.dt = read_value<double>(input_);
    config_.seed = read_value<std::uint32_t>(input_);
    if (!input_ || !std::isfinite(config_.dt) || config_.dt <= 0 ||
        !std::isfinite(config_.dt * static_cast<double>(steps))) {
        throw std::runtime_error("Cabecera de trayectoria no válida.");
    }
    const auto bytes = frame_bytes(count, steps);
    const auto expected = header_bytes + (steps + 1) * bytes;
    input_.seekg(0, std::ios::end);
    if (!input_ || input_.tellg() != static_cast<std::streamoff>(expected)) {
        throw std::runtime_error("Trayectoria truncada o con datos adicionales.");
    }
    count_ = static_cast<std::size_t>(count);
    config_.bodies_per_galaxy = count_ / 2;
    config_.steps = static_cast<std::size_t>(steps);
    buffer_.resize(bytes);
}

std::vector<Body> TrajectoryReader::read_frame(std::size_t frame) {
    if (frame >= frame_count()) throw std::out_of_range("Frame fuera de la trayectoria.");
    input_.seekg(static_cast<std::streamoff>(header_bytes + frame * buffer_.size()));
    input_.read(buffer_.data(), static_cast<std::streamsize>(buffer_.size()));
    if (!input_) throw std::runtime_error("No se pudo leer el frame.");
    std::vector<Body> bodies(count_);
    const char* cursor = buffer_.data();
    for (Body& body : bodies) {
        double values[7];
        std::int32_t galaxy;
        std::memcpy(values, cursor, sizeof(values)); cursor += sizeof(values);
        std::memcpy(&galaxy, cursor, sizeof(galaxy)); cursor += sizeof(galaxy);
        for (double value : values) {
            if (!std::isfinite(value)) throw std::runtime_error("Estado no finito en la trayectoria.");
        }
        if (values[0] <= 0 || (galaxy != 0 && galaxy != 1)) {
            throw std::runtime_error("Masa o galaxia no válida en la trayectoria.");
        }
        body = {values[0], {values[1], values[2]}, {values[3], values[4]},
                {values[5], values[6]}, galaxy};
    }
    return bodies;
}
}  // namespace nbody
