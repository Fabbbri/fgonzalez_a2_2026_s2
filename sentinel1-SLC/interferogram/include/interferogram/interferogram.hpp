#pragma once
#include "complex.hpp"
#include <vector>

namespace interferogram {
/*
 * Exige vectores no vacíos, del mismo tamaño y una salida distinta de las entradas.
 * Lanza std::invalid_argument si falla el contrato; llamar antes de cronometrar.
 */
void validateInterferogramInputs(const std::vector<Complex>& a, 
const std::vector<Complex>& b, const std::vector<Complex>& result);
/*
 * Sobrescribe result con a * conjugado(b), muestra por muestra, en tiempo O(N).
 * Requiere entradas y salida verificadas con validateInterferogramInputs.
 */

void computeInterferogramSequential(const std::vector<Complex>& a,
const std::vector<Complex>& b, std::vector<Complex>& result);
// Coordenadas locales a la ventana;
struct Tile { int xBegin, yBegin, xEnd, yEnd; };
// Genera un tile sin solapamientos;
std::vector<Tile> makeTiles(int width, int height, int tileWidth, int tileHeight);
void computeInterferogramTiled(const std::vector<Complex>& a,
                              const std::vector<Complex>& b,
                              std::vector<Complex>& result,
                              int width, const std::vector<Tile>& tiles);
} // namespace interferogram
