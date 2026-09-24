#pragma once
#include <iosfwd>
#include <string>

namespace interferogram {
/*
 * Agrupa las rutas de las adquisiciones y la ventana común que se procesará.
 * Las rutas predeterminadas se resuelven desde el directorio sentinel1-SLC/.
 */
struct Config {
    std::string a = "data/raw/P09-R2/S1A_SLC_20250110T112318_179298_IW2_VV_462858.SAFE/measurement/s1a-iw2-slc-vv-179298-20250110T112318-462858.tiff";
    std::string b = "data/raw/P09-R2/S1A_SLC_20250122T112317_179298_IW2_VV_464635.SAFE/measurement/s1a-iw2-slc-vv-179298-20250122T112317-464635.tiff";
    // Origen en píxeles, con índices de columna y fila desde cero.
    int x = 0;
    int y = 0;
    // Tamaño de la ventana; no modifica las dimensiones de los TIFF originales.
    int width = 512;
    int height = 512;
    bool help = false;
};
/*
 * Interpreta argc y argv conservando las opciones predeterminadas no indicadas.
 * Devuelve la configuración o lanza std::invalid_argument por argumentos inválidos.
 */
Config parseConfig(int argc, char* argv[]);
/** Escribe las opciones de ejecución en el flujo out. */
void printHelp(std::ostream& out);
} // namespace interferogram
