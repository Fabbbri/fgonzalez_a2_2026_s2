/**
 * @file bitmap_text.hpp
 * Fuente binaria de cinco columnas y siete filas sin archivos externos.
 * Cada entero codifica cinco bits de una fila; draw_text dibuja los bits activos
 * como rectángulos SDL. Pertenece a la visualización secuencial anterior.
 */
#pragma once
#include <SDL.h>
#include <array>
#include <cctype>
#include <string_view>

namespace nbody {
// Fuente mínima 5x7: cada fila usa cinco bits. No requiere fuentes externas.
// Traduce un carácter a siete enteros-fila. Cada bit activo indica un píxel
// del glifo: el bit 4 es la columna izquierda y el bit 0, la derecha.
inline std::array<unsigned char, 7> glyph(char character) {
    // Cada retorno codifica un patrón 5x7; el carácter no reconocido produce un glifo vacío.
    switch (character) {
    case 'A': return {14,17,17,31,17,17,17};
    case 'B': return {30,17,17,30,17,17,30};
    case 'C': return {14,17,16,16,16,17,14};
    case 'D': return {30,17,17,17,17,17,30};
    case 'E': return {31,16,16,30,16,16,31};
    case 'F': return {31,16,16,30,16,16,16};
    case 'G': return {14,17,16,23,17,17,15};
    case 'H': return {17,17,17,31,17,17,17};
    case 'I': return {14,4,4,4,4,4,14};
    case 'J': return {7,2,2,2,18,18,12};
    case 'K': return {17,18,20,24,20,18,17};
    case 'L': return {16,16,16,16,16,16,31};
    case 'M': return {17,27,21,21,17,17,17};
    case 'N': return {17,25,21,19,17,17,17};
    case 'O': return {14,17,17,17,17,17,14};
    case 'P': return {30,17,17,30,16,16,16};
    case 'Q': return {14,17,17,17,21,18,13};
    case 'R': return {30,17,17,30,20,18,17};
    case 'S': return {15,16,16,14,1,1,30};
    case 'T': return {31,4,4,4,4,4,4};
    case 'U': return {17,17,17,17,17,17,14};
    case 'V': return {17,17,17,17,17,10,4};
    case 'W': return {17,17,17,21,21,21,10};
    case 'X': return {17,17,10,4,10,17,17};
    case 'Y': return {17,17,10,4,4,4,4};
    case 'Z': return {31,1,2,4,8,16,31};
    case '0': return {14,17,19,21,25,17,14};
    case '1': return {4,12,4,4,4,4,14};
    case '2': return {14,17,1,2,4,8,31};
    case '3': return {30,1,1,14,1,1,30};
    case '4': return {2,6,10,18,31,2,2};
    case '5': return {31,16,16,30,1,1,30};
    case '6': return {14,16,16,30,17,17,14};
    case '7': return {31,1,2,4,8,8,8};
    case '8': return {14,17,17,14,17,17,14};
    case '9': return {14,17,17,15,1,1,14};
    case '.': return {0,0,0,0,0,12,12};
    case ':': return {0,12,12,0,12,12,0};
    case '-': return {0,0,0,31,0,0,0};
    case '+': return {0,4,4,31,4,4,0};
    case '/': return {1,2,2,4,8,8,16};
    default: return {};
    }
}

// Rasteriza glifos como rectángulos de scale por scale píxeles en SDL.
inline void draw_text(SDL_Renderer* renderer, std::string_view text, int x, int y,
                      SDL_Color color, int scale = 2) {
    // Color RGBA que se aplica a todos los píxeles del texto.
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    // Recorre bytes con tipo sin signo para llamar toupper dentro de su dominio válido.
    for (unsigned char character : text) {
        // Mayúscula correspondiente y patrón binario para el carácter.
        const auto rows = glyph(static_cast<char>(std::toupper(character)));
        // Recorre las siete filas; el bucle column recorre los cinco bits significativos.
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                // Extrae el bit de columna con AND y desplazamiento; solo dibuja valores uno.
                if ((rows[row] & (1 << (4 - column))) != 0) {
                    // Escala la celda del glifo y la traslada a la posición de pantalla.
                    const SDL_Rect pixel{x + column * scale, y + row * scale, scale, scale};
                    SDL_RenderFillRect(renderer, &pixel);
                }
            }
        }
        // Avanza cinco columnas más una de separación entre caracteres.
        x += 6 * scale;
    }
}
}  // namespace nbody
