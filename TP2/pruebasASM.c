#include <stdio.h>
extern long sumar_uno(long valor);
/**
 * @brief Recibe un valor GINI como float, lo convierte a entero y le suma 1.
 * @param valor El índice GINI como número con decimales (ej: 42.7)
 * @return El valor convertido a entero más 1 (ej: 43)
 */
long procesar_gini(double valor) {
    long entero = (long) valor;
    return sumar_uno(entero);
}

int main() { 
    double gini = 42.7;
    long resultado = procesar_gini(gini);
    printf("El resultado es: %ld\n", resultado);
    return 0;
 }
