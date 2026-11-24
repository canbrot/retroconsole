#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <linux/input.h>

int main(int argc, char *argv[])
{
    char ruta[64];
    char nombre[128];
    int fd, i, segundos = 10;
    int total = 0, botones = 0, ejes = 0;
    struct input_event ev;
    time_t inicio;

    if (argc < 2) {
        printf("Dispositivos de entrada:\n");
        for (i = 0; i < 32; i++) {
            sprintf(ruta, "/dev/input/event%d", i);
            fd = open(ruta, O_RDONLY);
            if (fd < 0)
                continue;
            strcpy(nombre, "?");
            ioctl(fd, EVIOCGNAME(sizeof(nombre)), nombre);
            printf("%s  %s\n", ruta, nombre);
            close(fd);
        }
        return 0;
    }

    if (argc > 2)
        segundos = atoi(argv[2]);
    fd = open(argv[1], O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    strcpy(nombre, "?");
    ioctl(fd, EVIOCGNAME(sizeof(nombre)), nombre);
    printf("Leyendo %s (%s) durante %d s. Pulsa botones.\n", argv[1], nombre, segundos);

    inicio = time(NULL);
    while (time(NULL) - inicio < segundos) {
        fd_set conj;
        struct timeval espera;
        FD_ZERO(&conj);
        FD_SET(fd, &conj);
        espera.tv_sec = 1;
        espera.tv_usec = 0;
        if (select(fd + 1, &conj, NULL, NULL, &espera) <= 0)
            continue;
        if (read(fd, &ev, sizeof(ev)) != sizeof(ev))
            continue;
        if (ev.type == EV_SYN)
            continue;
        total++;
        if (ev.type == EV_KEY && ev.value == 1)
            botones++;
        if (ev.type == EV_ABS)
            ejes++;
        if (ev.type == EV_KEY || total <= 20)
            printf("%ld.%06ld tipo=%d codigo=%d valor=%d\n",
                   (long)ev.time.tv_sec, (long)ev.time.tv_usec,
                   ev.type, ev.code, ev.value);
    }
    printf("Resumen: eventos=%d botones_pulsados=%d eventos_de_ejes=%d en %d s\n",
           total, botones, ejes, segundos);
    close(fd);
    return 0;
}
