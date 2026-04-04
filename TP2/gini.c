#include <stdio.h>

/**
 * @brief Recibe un valor GINI como float, lo convierte a entero y le suma 1.
 * @param valor El índice GINI como número con decimales (ej: 42.7)
 * @return El valor convertido a entero más 1 (ej: 43)
 */
long procesar_gini(double valor) {
    long entero = (long) valor;
    return entero + 1;
}