#pragma once
#include "complex.hpp"
#include <string>
#include <vector>

namespace interferogram {
// Dimensiones de los bursts seleccionados para P09-R2.
inline constexpr int slcWidth = 24445;
inline constexpr int slcHeight = 1500;
/*
 * Valida el origen (x, y) en base cero y las dimensiones positivas de la ventana.
 * Lanza std::invalid_argument si el rectángulo excede los límites del burst.
 */
void validateWindow(int x, int y, int width, int height);
/*
 * Lee de path una ventana de una banda compleja de slcWidth × slcHeight.
 * Devuelve muestras por filas: índice = fila * width + columna.
 * Lanza std::invalid_argument por ventanas inválidas o std::runtime_error por
 * errores de apertura, formato o lectura. Conserva real e imaginario sin corregistro.
 */
std::vector<Complex> readSlcWindow(const std::string& path,
                                 int x, int y, int width, int height);
} 
