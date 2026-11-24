#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <sys/reboot.h>

static volatile sig_atomic_t apagar = 0;
static int usb_montado = 0;

static void manejador_senal(int sig)
{
    (void)sig;
    apagar = 1;
}

static void escribir_parametro(const char *ruta, const char *valor)
{
    int fd = open(ruta, O_WRONLY);
    if (fd >= 0) {
        ssize_t r = write(fd, valor, strlen(valor));
        (void)r;
        close(fd);
    }
}

static void configurar_kernel(void)
{
    char ruta[128];
    escribir_parametro("/sys/block/sda/queue/scheduler", "bfq");
    escribir_parametro("/proc/sys/vm/swappiness", "1");

    for (int cpu = 0; cpu < 8; cpu++) {
        snprintf(ruta, sizeof(ruta),
                 "/sys/devices/system/cpu/cpu%d/cpufreq/scaling_governor", cpu);
        if (access(ruta, W_OK) == 0)
            escribir_parametro(ruta, "performance");
    }
}

static void montar_usb_roms(void)
{
    const char *dispositivos[] = {
        "/dev/sdb1", "/dev/sdc1", "/dev/sdd1",
        "/dev/sdb",  "/dev/sdc",  "/dev/sdd"
    };
    const char *fs_tipos[] = { "vfat", "exfat", "ext4" };
    const char *punto = "/home/canbrot/roms/usb";

    mkdir("/home/canbrot/roms", 0755);
    mkdir(punto, 0755);

    for (size_t i = 0; i < sizeof(dispositivos) / sizeof(dispositivos[0]); i++) {
        if (access(dispositivos[i], F_OK) != 0)
            continue;
        for (size_t j = 0; j < sizeof(fs_tipos) / sizeof(fs_tipos[0]); j++) {
            if (mount(dispositivos[i], punto, fs_tipos[j], MS_RDONLY, NULL) == 0) {
                usb_montado = 1;
                printf("myinit: USB %s montado en %s (%s)\n",
                       dispositivos[i], punto, fs_tipos[j]);
                return;
            }
        }
    }
}

static pid_t lanzar_consola(void)
{
    pid_t pid = fork();
    if (pid == 0) {
        char *args[] = { "/usr/local/bin/lanzar.sh", NULL };
        execv(args[0], args);
        perror("myinit: execv lanzar.sh");
        _exit(127);
    }
    return pid;
}

int main(void)
{
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    mkdir("/dev/pts", 0755);
    mount("devpts", "/dev/pts", "devpts", 0, NULL);
    mount("tmpfs", "/run", "tmpfs", 0, "mode=0755");
    mount("tmpfs", "/tmp", "tmpfs", 0, "mode=1777");

    signal(SIGTERM, manejador_senal);
    signal(SIGINT, manejador_senal);
    reboot(RB_DISABLE_CAD);

    configurar_kernel();
    sleep(2);
    montar_usb_roms();

    printf("myinit: PID %d activo, iniciando entorno de consola\n", getpid());

    int fallos = 0;
    pid_t hijo = lanzar_consola();
    if (hijo < 0) {
        perror("myinit: fork");
        reboot(RB_POWER_OFF);
        return 1;
    }

    while (!apagar) {
        int estado = 0;
        pid_t quien = waitpid(-1, &estado, 0);

        if (quien < 0) {
            if (errno == EINTR)
                continue;
            sleep(1);
            continue;
        }

        if (quien == hijo) {
            if (WIFEXITED(estado) && WEXITSTATUS(estado) == 0) {
                printf("myinit: sesion cerrada por el usuario, apagando\n");
                apagar = 1;
            } else {
                fallos++;
                printf("myinit: sesion termino con error (intento %d/3)\n", fallos);
                if (fallos >= 3) {
                    printf("myinit: demasiados reinicios, apagando\n");
                    apagar = 1;
                } else {
                    sleep(1);
                    hijo = lanzar_consola();
                }
            }
        }
    }

    printf("myinit: sincronizando sistema y apagando\n");
    if (usb_montado)
        umount2("/home/canbrot/roms/usb", MNT_DETACH);

    kill(-1, SIGTERM);
    sleep(1);
    kill(-1, SIGKILL);
    sync();
    mount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, NULL);
    reboot(RB_POWER_OFF);

    return 0;
}
