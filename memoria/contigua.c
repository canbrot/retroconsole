#include <stdio.h>

#define MEM_TOTAL 512

// Modos: 0 = First-Fit, 1 = Best-Fit, 2 = Worst-Fit
int asignar(int mem[], int modo, int id, int tam) {
    int mejor_ini = -1;
    int mejor_tam = (modo == 1) ? MEM_TOTAL + 1 : -1;

    int i = 0;
    while (i < MEM_TOTAL) {
        if (mem[i] != 0) { i++; continue; }
        int ini = i;
        while (i < MEM_TOTAL && mem[i] == 0) i++;
        int hueco = i - ini;

        if (hueco >= tam) {
            if (modo == 0) {
                mejor_ini = ini;
                break;
            } else if (modo == 1 && hueco < mejor_tam) {
                mejor_tam = hueco;
                mejor_ini = ini;
            } else if (modo == 2 && hueco > mejor_tam) {
                mejor_tam = hueco;
                mejor_ini = ini;
            }
        }
    }

    if (mejor_ini != -1) {
        for (int j = mejor_ini; j < mejor_ini + tam; j++) mem[j] = id;
    }
    return mejor_ini;
}

void liberar(int mem[], int id) {
    for (int i = 0; i < MEM_TOTAL; i++) {
        if (mem[i] == id) mem[i] = 0;
    }
}

void simular(const char *nombre, int modo) {
    int mem[MEM_TOTAL] = {0};
    // Carga sintetica: >0 asigna (id, bloques), <0 libera (-id)
    int ops[][2] = {
        {1, 60},  {2, 90},  {3, 130}, {4, 40},  {5, 50},
        {-2, 0},  {-4, 0},
        {6, 35},  {7, 80},
        {-3, 0},
        {8, 120},
        {9, 150}  // prueba fragmentacion externa con >150 bloques libres totales
    };
    int n_ops = sizeof(ops) / sizeof(ops[0]);
    int fallos = 0;

    for (int k = 0; k < n_ops; k++) {
        int id = ops[k][0], tam = ops[k][1];
        if (id > 0) {
            if (asignar(mem, modo, id, tam) == -1) fallos++;
        } else {
            liberar(mem, -id);
        }
    }

    int libre_total = 0, mayor_hueco = 0, actual = 0, huecos = 0;
    for (int i = 0; i < MEM_TOTAL; i++) {
        if (mem[i] == 0) {
            libre_total++;
            actual++;
            if (actual == 1) huecos++;
            if (actual > mayor_hueco) mayor_hueco = actual;
        } else {
            actual = 0;
        }
    }

    printf("%-10s | fallos=%d | libre_total=%3d | mayor_hueco=%3d | huecos=%d\n",
           nombre, fallos, libre_total, mayor_hueco, huecos);
}

int main(void) {
    printf("Simulacion asignacion contigua (512 bloques sinteticos)\n");
    simular("First-Fit", 0);
    simular("Best-Fit",  1);
    simular("Worst-Fit", 2);
    return 0;
}
