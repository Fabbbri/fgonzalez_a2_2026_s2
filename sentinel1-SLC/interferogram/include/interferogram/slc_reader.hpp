#pragma once
#include "complex.hpp"
#include <string>
#include <vector>

namespace interferogram {
/*
 * Valida el origen (x, y) en base cero y las dimensiones positivas de la ventana.
 * Lanza std::invalid_argument por origen negativo o tamaño no positivo.
 */
void validateWindow(int x, int y, int width, int height);
/*
 * Lee de path una ventana de una banda compleja, validando los límites del archivo.
 * allowNoData conserva componentes no finitas; por defecto se rechazan.
 * Devuelve muestras por filas: índice = fila * width + columna.
 * Lanza std::invalid_argument por ventanas inválidas o std::runtime_error por
 * errores de apertura, formato o lectura. Conserva real e imaginario sin corregistro.
 */
std::vector<Complex> readSlcWindow(const std::string& path,
                                 int x, int y, int width, int height, bool allowNoData = false);
} 
