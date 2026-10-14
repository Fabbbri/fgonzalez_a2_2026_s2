#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <regex>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
struct Sample { double totalMs; std::string smt; };
struct Case {
    std::string model;
    int threads;
    std::vector<Sample> samples;
    Case(std::string name, int count) : model(std::move(name)), threads(count) {}
};

std::string read(const fs::path& path) {
    std::ifstream file(path);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

std::string smtState() {
    const auto state = read("/sys/devices/system/cpu/smt/active");
    return state.empty() ? "unknown" : state.substr(0, 1);
}

double execute(const std::vector<std::string>& command, const fs::path& log) {
    const auto start = std::chrono::steady_clock::now();
    const pid_t child = fork();
    if (child == 0) {
        FILE* file = std::fopen(log.c_str(), "w");
        if (!file || dup2(fileno(file), STDOUT_FILENO) < 0 || dup2(fileno(file), STDERR_FILENO) < 0) _exit(126);
        std::fclose(file);
        std::vector<char*> args;
        for (const auto& value : command) args.push_back(const_cast<char*>(value.c_str()));
        args.push_back(nullptr);
        execv(args[0], args.data());
        _exit(127);
    }
    if (child < 0) throw std::runtime_error("No se pudo iniciar el proceso");
    int status = 0;
    pid_t result;
    do { result = waitpid(child, &status, 0); } while (result < 0 && errno == EINTR);
    const double milliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    if (result < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
        throw std::runtime_error("Falló el ejecutable: " + log.string());
    return milliseconds;
}

std::string topology() {
    std::string signature = read("/sys/devices/system/cpu/online") + smtState() +
        read("/sys/devices/system/cpu/smt/control");
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) if (line.rfind("Cpus_allowed_list:", 0) == 0) signature += line;
    return signature;
}

void writeModel(const Case& item, const fs::path& output, double sequentialMean) {
    const auto path = output / (item.model + ".csv");
    const bool newFile = !fs::exists(path);
    std::ofstream csv(path, std::ios::app);
    if (!csv) throw std::runtime_error("No se pudo escribir " + path.string());
    if (newFile) csv << "modelo,hilos,repeticion,tiempo_total_ms,tiempo_secuencial_ms,smt_activo_linux\n";
    csv << std::setprecision(17);
    for (std::size_t i = 0; i < item.samples.size(); ++i) {
        const auto& sample = item.samples[i];
        csv << item.model << ',' << item.threads << ',' << i + 1 << ',' << sample.totalMs
            << ',' << sequentialMean << ',' << sample.smt << '\n';
    }
    csv.flush();
    if (!csv) throw std::runtime_error("Falló la escritura de " + path.string());
}

int main(int argc, char** argv) {
    try {
        if (argc < 4) throw std::invalid_argument("Uso: benchmark SALIDA REPETICIONES EJECUTABLE argumentos_comunes...");
        const fs::path output = argv[1];
        const int repetitions = std::stoi(argv[2]);
        if (repetitions < 2) throw std::invalid_argument("Se requieren al menos dos repeticiones");
        if (fs::exists(output)) throw std::invalid_argument("El directorio de salida debe ser nuevo");
        fs::create_directories(output);
        std::ofstream(output / "status.txt") << "incomplete\n";
        std::vector<std::string> common;
        for (int i = 4; i < argc; ++i) {
            const std::string argument = argv[i];
            if (argument == "--mode" || argument == "--threads" || argument == "--help")
                throw std::invalid_argument("La campaña controla --mode y --threads; use solo argumentos comunes");
            common.push_back(argument);
        }
        std::vector<Case> cases{{"sequential", 1}, {"fine-grained", 1},
            {"coarse-grained", 1}, {"coarse-grained", 2}, {"coarse-grained", 4}, {"coarse-grained", 8},
            {"smt", 8}, {"smt", 12}, {"cmp", 1}, {"cmp", 2}, {"cmp", 4}, {"cmp", 8}};
        std::ofstream configuration(output / "configuration.txt");
        configuration << "repetitions=" << repetitions << "\nwarmups=3\ncommand=" << argv[3] << '\n';
        for (const auto& arg : common) configuration << std::quoted(arg) << ' ';
        configuration << '\n';
        const auto originalTopology = topology();
        const std::regex kernelRegex("Kernel time: ([0-9.eE+-]+) ms");
        const std::regex sumRegex("Result sum[^\\n]+");
        const std::regex workerRegex("Workers: ([0-9]+)");
        std::string checksum;
        double sequentialMean = 0;
        for (auto& item : cases) {
            std::vector<std::string> command{fs::absolute(argv[3]).string(), "--mode", item.model};
            if (item.model != "sequential" && item.model != "fine-grained") {
                command.insert(command.end(), {"--threads", std::to_string(item.threads)});
            }
            command.insert(command.end(), common.begin(), common.end());
            for (int run = -3; run < repetitions; ++run) {
                if (topology() != originalTopology) throw std::runtime_error("Cambió la topología/afinidad; lote incompleto");
                const std::string phase = run < 0 ? "warmup" : "measurement";
                const int number = run < 0 ? run + 4 : run + 1;
                const auto log = output / (phase + '-' + item.model + '-' + std::to_string(item.threads) + '-' + std::to_string(number) + ".log");
                const double elapsed = execute(command, log);
                if (topology() != originalTopology) throw std::runtime_error("Cambió la topología/afinidad durante una ejecución; lote incompleto");
                const auto text = read(log);
                std::smatch match;
                if (!std::regex_search(text, match, kernelRegex)) throw std::runtime_error("Falta tiempo del kernel: " + log.string());
                const double kernel = std::stod(match[1]);
                if (!std::isfinite(kernel) || kernel <= 0 || !std::isfinite(elapsed) || elapsed <= 0)
                    throw std::runtime_error("Tiempo inválido: " + log.string());
                if (!std::regex_search(text, match, sumRegex)) throw std::runtime_error("Falta suma del resultado: " + log.string());
                if (checksum.empty()) checksum = match[0];
                else if (checksum != match[0]) throw std::runtime_error("Resultados diferentes: " + log.string());
                int workers = 1;
                if (std::regex_search(text, match, workerRegex)) workers = std::stoi(match[1]);
                if (workers != item.threads) throw std::runtime_error("Cantidad de hilos incorrecta: " + log.string());
                const auto smt = smtState();
                if (run >= 0) item.samples.push_back({elapsed, smt});
                if (run < 0 || number % 10 == 0 || number == repetitions)
                    std::cout << item.model << ':' << workers << ' ' << phase << ' ' << number << '/'
                              << (run < 0 ? 3 : repetitions) << std::endl;
            }
            if (item.model == "sequential") {
                const double total = std::accumulate(item.samples.begin(), item.samples.end(), 0.0,
                    [](double sum, const Sample& sample) { return sum + sample.totalMs; });
                sequentialMean = total / item.samples.size();
            }
            writeModel(item, output, sequentialMean);
        }
        std::ofstream(output / "status.txt") << "complete\n";
        std::cout << "Resultados: " << output << '\n';
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
