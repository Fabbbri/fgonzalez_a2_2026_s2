#pragma once
#include "nbody/config.hpp"
#include "nbody/simulation.hpp"
#include <fstream>

namespace nbody {
// Formato binario v1: cabecera y estados completos de tamaño fijo, sin padding
// de estructuras C++. El lector mantiene solo un frame en memoria.
class TrajectoryWriter {
public:
    TrajectoryWriter(const std::string& path, const Config& config, std::size_t bodies);
    void append(const std::vector<Body>& bodies);
    void finish();  // Valida el número de frames y comprueba flush/cierre.
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
}  // namespace nbody
