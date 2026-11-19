#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <sys/reboot.h>

volatile int apagar = 0;

void manejador(int sig)
{
    apagar = 1;
}

pid_t lanzar(void)
{
    pid_t p = fork();
    if (p == 0) {
        setsid();
        execl("/bin/sh", "sh", "/usr/local/bin/lanzar.sh", (char *)NULL);
        perror("execl");
        _exit(1);
    }
    return p;
}

int main(void)
{
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    mount(NULL, "/", NULL, MS_REMOUNT, NULL);
    mount("tmpfs", "/tmp", "tmpfs", 0, NULL);
    mount("tmpfs", "/run", "tmpfs", 0, NULL);

    signal(SIGTERM, manejador);
    signal(SIGINT, manejador);
    reboot(RB_DISABLE_CAD);

    printf("myinit: PID %d listo\n", getpid());

    int veces = 0;
    pid_t hijo = lanzar();
    printf("myinit: lance el hijo %d\n", hijo);

    while (!apagar) {
        pid_t quien = waitpid(-1, NULL, 0);

        if (quien < 0) {
            sleep(1);
            continue;
        }

        if (quien == hijo) {
            veces++;
            printf("myinit: el hijo %d termino (%d)\n", quien, veces);
            if (veces >= 3) {
                printf("myinit: demasiados reinicios, apagando\n");
                apagar = 1;
                continue;
            }
            sleep(2);
            hijo = lanzar();
            printf("myinit: relance el hijo %d\n", hijo);
        } else {
            printf("myinit: recogi un huerfano %d\n", quien);
        }
    }

    printf("myinit: apagando, avisando a los procesos\n");
    kill(-1, SIGTERM);
    sleep(2);
    sync();
    mount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, NULL);
    printf("myinit: disco sincronizado, apagando\n");
    reboot(RB_POWER_OFF);

    return 0;
}
