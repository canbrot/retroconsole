#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include <sys/resource.h>

#define PERIODO_NS 16667000L

int cmp(const void *a, const void *b)
{
    long x = *(const long *)a, y = *(const long *)b;
    return (x > y) - (x < y);
}

int main(int argc, char *argv[])
{
    const char *modo = argc > 1 ? argv[1] : "normal";
    int frames = argc > 2 ? atoi(argv[2]) : 1800;
    long *jit;
    struct timespec next, now;
    struct sched_param sp;
    int i, tarde = 0;
    double suma = 0;

    if (strcmp(modo, "nice") == 0) {
        if (setpriority(PRIO_PROCESS, 0, -20) != 0) { perror("setpriority"); return 1; }
    } else if (strcmp(modo, "fifo") == 0) {
        sp.sched_priority = 50;
        if (sched_setscheduler(0, SCHED_FIFO, &sp) != 0) { perror("sched_setscheduler"); return 1; }
    }
    jit = malloc(sizeof(long) * frames);
    if (jit == NULL) return 1;

    clock_gettime(CLOCK_MONOTONIC, &next);
    for (i = 0; i < frames; i++) {
        next.tv_nsec += PERIODO_NS;
        while (next.tv_nsec >= 1000000000L) { next.tv_nsec -= 1000000000L; next.tv_sec++; }
        clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
        clock_gettime(CLOCK_MONOTONIC, &now);
        jit[i] = (now.tv_sec - next.tv_sec) * 1000000000L + (now.tv_nsec - next.tv_nsec);
        suma += jit[i];
        if (jit[i] > 1000000L) tarde++;
    }
    qsort(jit, frames, sizeof(long), cmp);
    printf("modo=%-6s frames=%d | min=%.0f prom=%.0f p99=%.0f max=%.0f us | tarde>1ms=%d\n",
           modo, frames, jit[0] / 1000.0, suma / frames / 1000.0,
           jit[(int)(frames * 0.99)] / 1000.0, jit[frames - 1] / 1000.0, tarde);
    free(jit);
    return 0;
}
