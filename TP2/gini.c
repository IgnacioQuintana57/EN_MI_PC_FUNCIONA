#include <stdio.h>

extern long procesar_gini(double valor);

int main() {
    /* Valores reales del índice GINI de Argentina (2011-2020)
     * obtenidos de la API del Banco Mundial.
     * Se hardcodean para facilitar la depuración con GDB,
     * tal como recomiendan. */
    double valores[] = {42.7, 41.4, 41.1, 41.8, 42.3, 41.4, 41.7, 43.3, 42.7};
    int anios[]      = {2011, 2012, 2013, 2014, 2016, 2017, 2018, 2019, 2020};
    int n = 9;

    for (int i = 0; i < n; i++) {
        long resultado = procesar_gini(valores[i]);
        printf("Año %d: GINI=%.1f -> Procesado=%ld\n", anios[i], valores[i], resultado);
    }
    return 0;
}