#pragma once
#include <iosfwd>
#include <string>

namespace interferogram {
/*
 * Agrupa las rutas de las adquisiciones y la ventana común que se procesará.
 * Las rutas predeterminadas se resuelven desde el directorio sentinel1-SLC/.
 */
struct Config {
    std::string a = "data/coregistered/P09-R2-2023-2024/A.tif";
    std::string b = "data/coregistered/P09-R2-2023-2024/B.tif";
    // Origen en píxeles, con índices de columna y fila desde cero.
    int x = 0;
    int y = 0;
    // Tamaño de la ventana; no modifica las dimensiones de los TIFF originales.
    int width = 24444;
    int height = 1499;
    bool allowNoData = false; // --allow-nodata es necesario si la ventana incluye bordes NoData.
    std::string mode = "sequential";
    int tileWidth = 32;
    int tileHeight = 32;
    int quantum = 1; // Píxeles por turno cooperativo.
    int threads = 2; // Trabajadores de coarse-grained.
    bool threadsSpecified = false; // SMT usa 2*C si no se indica --threads.
    std::string output; // TIFF complejo opcional con el resultado del kernel.
    bool help = false;
};
/*
 * Interpreta argc y argv conservando las opciones predeterminadas no indicadas.
 * Devuelve la configuración o lanza std::invalid_argument por argumentos inválidos.
 */
Config parseConfig(int argc, char* argv[]);
/* Escribe las opciones de ejecución en el flujo out. */
void printHelp(std::ostream& out);
} // namespace interferogram
