#include <stdio.h>
#include <stdlib.h>
#define MAXREF 400000
#define MAXF 128
int ref[MAXREF];
int n;

void add(int p) { if (n < MAXREF) ref[n++] = p; }

void generar(int estres)
{
    int f, i, k;
    n = 0;
    srand(42);
    for (f = 0; f < 2000; f++) {
        for (i = 0; i < 8; i++) add(i);
        for (i = 0; i < 23; i++) add(100 + i);
        for (k = 0; k < 3; k++) add(200 + rand() % 32);
        if (estres && f % 10 == 0)
            for (i = 0; i < 50; i++) add(1000 + ((f / 10) * 50 + i) % 500);
    }
}

int buscar(int *mem, int cnt, int p)
{
    int i;
    for (i = 0; i < cnt; i++) if (mem[i] == p) return i;
    return -1;
}

int fifo(int frames)
{
    int mem[MAXF], cnt = 0, head = 0, fallos = 0, t;
    for (t = 0; t < n; t++) {
        if (buscar(mem, cnt, ref[t]) >= 0) continue;
        fallos++;
        if (cnt < frames) mem[cnt++] = ref[t];
        else { mem[head] = ref[t]; head = (head + 1) % frames; }
    }
    return fallos;
}

int lru(int frames)
{
    int mem[MAXF], uso[MAXF], cnt = 0, fallos = 0, t, i, v, pos;
    for (t = 0; t < n; t++) {
        pos = buscar(mem, cnt, ref[t]);
        if (pos >= 0) { uso[pos] = t; continue; }
        fallos++;
        if (cnt < frames) { mem[cnt] = ref[t]; uso[cnt] = t; cnt++; }
        else {
            v = 0;
            for (i = 1; i < cnt; i++) if (uso[i] < uso[v]) v = i;
            mem[v] = ref[t]; uso[v] = t;
        }
    }
    return fallos;
}

int clock_alg(int frames)
{
    int mem[MAXF], bit[MAXF], cnt = 0, mano = 0, fallos = 0, t, pos;
    for (t = 0; t < n; t++) {
        pos = buscar(mem, cnt, ref[t]);
        if (pos >= 0) { bit[pos] = 1; continue; }
        fallos++;
        if (cnt < frames) { mem[cnt] = ref[t]; bit[cnt] = 1; cnt++; }
        else {
            while (bit[mano]) { bit[mano] = 0; mano = (mano + 1) % frames; }
            mem[mano] = ref[t]; bit[mano] = 1;
            mano = (mano + 1) % frames;
        }
    }
    return fallos;
}

int main(void)
{
    int marcos[] = {4, 8, 16, 32, 48, 64};
    int e, m;
    for (e = 0; e < 2; e++) {
        generar(e);
        printf("=== Escenario %s: %d accesos ===\n", e ? "CON ESTRES" : "SIN ESTRES", n);
        printf("marcos   FIFO    LRU  CLOCK  (fallos de pagina)\n");
        for (m = 0; m < 6; m++)
            printf("%6d %6d %6d %6d\n", marcos[m], fifo(marcos[m]),
                   lru(marcos[m]), clock_alg(marcos[m]));
    }
    return 0;
}
