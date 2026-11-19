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

int main(void)
{
    if (mount("proc", "/proc", "proc", 0, NULL) < 0)
        perror("mount /proc");
    if (mount("sysfs", "/sys", "sysfs", 0, NULL) < 0)
        perror("mount /sys");
    if (mount("devtmpfs", "/dev", "devtmpfs", 0, NULL) < 0)
        perror("mount /dev");

    signal(SIGTERM, manejador);
    signal(SIGINT, manejador);
    reboot(RB_DISABLE_CAD);

    printf("myinit: PID %d listo\n", getpid());

    int veces = 0;
    pid_t hijo = fork();
    if (hijo == 0) {
        execl("/bin/sleep", "sleep", "3", (char *)NULL);
        perror("execl");
        return 1;
    }
    printf("myinit: lance el hijo %d\n", hijo);

    while (!apagar) {
        int estado;
        pid_t quien = waitpid(-1, &estado, 0);

        if (quien < 0) {
            sleep(1);
            continue;
        }

        if (quien == hijo) {
            veces++;
            printf("myinit: el hijo %d termino, relanzando (%d)\n", quien, veces);
            if (veces >= 5) {
                printf("myinit: demo terminada\n");
                apagar = 1;
                continue;
            }
            hijo = fork();
            if (hijo == 0) {
                execl("/bin/sleep", "sleep", "3", (char *)NULL);
                perror("execl");
                return 1;
            }
            printf("myinit: lance el hijo %d\n", hijo);
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
