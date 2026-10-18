/**
 * @file tiles.hpp
 * Partición de la imagen en rectángulos de píxeles sin superposición.
 * Los límites son semiabiertos: [x0, x1) por [y0, y1).
 * El índice global del tile es la variable que reparten los modelos paralelos.
 */
#pragma once

#include <cstddef>

namespace rt {
// Límites superiores exclusivos: también describen los tiles del borde.
// x0,y0 incluidos; x1,y1 excluidos. Área = (x1-x0)*(y1-y0).
struct Tile { int x0, y0, x1, y1; };
// Grilla implícita: conserva dimensiones y calcula rectángulos bajo demanda.
class TileGrid {
public:
    // Valida dimensiones y obtiene filas y columnas mediante división hacia arriba.
    TileGrid(int width, int height, int tile_width, int tile_height);
    // Cantidad M de tareas independientes, sin depender del número P de hilos.
    std::size_t size() const;
    // Traduce el índice global de tarea a sus límites geométricos.
    Tile at(std::size_t index) const;
private:
    // Resolución, tamaño nominal de tile y número calculado de columnas y filas.
    int width_, height_, tile_width_, tile_height_, columns_, rows_;
};
} // namespace rt
