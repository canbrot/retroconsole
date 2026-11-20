#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void leer_meminfo(void)
{
    FILE *f = fopen("/proc/meminfo", "r");
    char linea[256];
    long total = 0, libre = 0, disp = 0, buffers = 0, cached = 0;
    long swaptotal = 0, swaplibre = 0;

    if (f == NULL) {
        perror("meminfo");
        return;
    }
    while (fgets(linea, sizeof(linea), f) != NULL) {
        if (strncmp(linea, "MemTotal:", 9) == 0) sscanf(linea + 9, "%ld", &total);
        if (strncmp(linea, "MemFree:", 8) == 0) sscanf(linea + 8, "%ld", &libre);
        if (strncmp(linea, "MemAvailable:", 13) == 0) sscanf(linea + 13, "%ld", &disp);
        if (strncmp(linea, "Buffers:", 8) == 0) sscanf(linea + 8, "%ld", &buffers);
        if (strncmp(linea, "Cached:", 7) == 0) sscanf(linea + 7, "%ld", &cached);
        if (strncmp(linea, "SwapTotal:", 10) == 0) sscanf(linea + 10, "%ld", &swaptotal);
        if (strncmp(linea, "SwapFree:", 9) == 0) sscanf(linea + 9, "%ld", &swaplibre);
    }
    fclose(f);

    printf("=== Memoria del sistema (MiB) ===\n");
    printf("Total:       %ld\n", total / 1024);
    printf("Libre:       %ld\n", libre / 1024);
    printf("Disponible:  %ld\n", disp / 1024);
    printf("Usada:       %ld\n", (total - disp) / 1024);
    printf("Buffers:     %ld\n", buffers / 1024);
    printf("Cache:       %ld\n", cached / 1024);
    printf("Swap total:  %ld\n", swaptotal / 1024);
    printf("Swap usada:  %ld\n", (swaptotal - swaplibre) / 1024);
}

void leer_proceso(int pid)
{
    char ruta[64];
    char linea[256];
    char nombre[64] = "?";
    long vsz = 0, rss = 0;
    FILE *f;

    sprintf(ruta, "/proc/%d/status", pid);
    f = fopen(ruta, "r");
    if (f == NULL) {
        printf("No existe el proceso %d\n", pid);
        return;
    }
    while (fgets(linea, sizeof(linea), f) != NULL) {
        if (strncmp(linea, "Name:", 5) == 0) sscanf(linea + 5, "%63s", nombre);
        if (strncmp(linea, "VmSize:", 7) == 0) sscanf(linea + 7, "%ld", &vsz);
        if (strncmp(linea, "VmRSS:", 6) == 0) sscanf(linea + 6, "%ld", &rss);
    }
    fclose(f);

    printf("=== Proceso %d (%s) ===\n", pid, nombre);
    printf("VSZ (reservada): %ld kB\n", vsz);
    printf("RSS (real):      %ld kB\n", rss);
}

int main(int argc, char *argv[])
{
    int pid = 1;

    if (argc > 1) {
        pid = atoi(argv[1]);
    }
    leer_meminfo();
    leer_proceso(pid);
    return 0;
}
