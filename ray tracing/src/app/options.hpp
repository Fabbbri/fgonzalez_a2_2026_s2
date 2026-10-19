/**
 * @file options.hpp
 * Parámetros de entrada y valores predeterminados de la aplicación.
 * Las dimensiones y los tiles definen la carga; threads define el número
 * de trabajadores sin modificar la partición geométrica.
 */
#pragma once

#include <string>
#include <vector>
#include "execution/execution.hpp"

namespace rt {
struct Options {
    // Resolución W,H y tamaño nominal Tw,Th; determinan M=ceil(W/Tw)*ceil(H/Th).
    int width = 640, height = 480, tile_width = 32, tile_height = 32;
    // Muestras y límite de rebotes: controlan el trabajo óptico por píxel.
    int samples = 128, max_bounces = 12;
    // Modos sin ventana, ayuda y consulta exclusiva de topología.
    bool headless = false, help = false, cpu_info = false;
    // Ruta PPM opcional; una cadena vacía desactiva escritura de imagen.
    std::string output;
    // Política de ejecución seleccionada, secuencial por defecto.
    ExecutionModel model = ExecutionModel::sequential;
    // K píxeles completos por turno, utilizado por fine.
    int quantum = 64;
    // Tiempo entre bloqueos y duración de cada espera de coarse, en milisegundos.
    int block_interval_ms = 10, block_duration_ms = 1;
    // P solicitado; cero es centinela interno de selección automática.
    int threads = 0; // 0 interno: automático según modelo. CLI acepta P positivo.
    // Activa campaña sin ventana en lugar de una única presentación.
    bool benchmark = false;
    // Cantidad de ejecuciones exportadas por configuración, excluyendo calentamiento.
    int repetitions = 200;
    // Barrido de P con carga y tiles fijos; vacío utiliza threads.
    std::vector<int> thread_counts;
    // Ruta del CSV crudo; el resumen agrega el sufijo -summary al nombre.
    std::string metrics_output = "results/metrics.csv";
    // Ruta opcional de referencia compartida con carga e imagen compatibles.
    std::string baseline_csv; // Referencia sequential compartida, sin volver a renderizarla.
};
// Interpreta y valida los argumentos; no inicializa escena ni ventana.
Options parse_options(int argc, char** argv);
} // namespace rt
