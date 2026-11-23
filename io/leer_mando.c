#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>

const char *nombre_tipo(unsigned short t) {
    if (t == EV_SYN) return "EV_SYN";
    if (t == EV_KEY) return "EV_KEY (boton)";
    if (t == EV_ABS) return "EV_ABS (eje/pad)";
    return "OTRO";
}

int main(int argc, char *argv[]) {
    const char *dev = argc > 1 ? argv[1] : "/dev/input/event16";
    int max_ev = argc > 2 ? atoi(argv[2]) : 25;

    int fd = open(dev, O_RDONLY);
    if (fd < 0) {
        perror("open");
        return 1;
    }

    printf("Leyendo hasta %d eventos de %s (pulsa botones en el mando)...\n", max_ev, dev);
    struct input_event ev;
    int leidos = 0;

    while (leidos < max_ev) {
        ssize_t n = read(fd, &ev, sizeof(ev));
        if (n != sizeof(ev)) break;
        printf("[%2d] ts=%ld.%06ld | tipo=%-16s (0x%02x) | code=%3u | value=%d\n",
               leidos + 1, ev.time.tv_sec, ev.time.tv_usec,
               nombre_tipo(ev.type), ev.type, ev.code, ev.value);
        leidos++;
    }

    close(fd);
    return 0;
}
