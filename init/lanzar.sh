#!/bin/sh
export HOME=/root
export PATH=/usr/sbin:/usr/bin:/sbin:/bin:/usr/games
mkdir -p /run/user/0
chmod 700 /run/user/0
export XDG_RUNTIME_DIR=/run/user/0

# 1. Carga de modulos de hardware y planificador BFQ
modprobe ehci_pci
modprobe usbhid
modprobe evdev
modprobe joydev
modprobe xpad
modprobe drm
modprobe nouveau
modprobe bfq

# 2. Politicas del kernel aplicadas por PID 1
[ -w /sys/block/sda/queue/scheduler ] && echo bfq > /sys/block/sda/queue/scheduler
for gov in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
  [ -w "$gov" ] && echo performance > "$gov"
done
sysctl -w vm.swappiness=1 >/dev/null 2>&1

UDEVD=/usr/lib/systemd/systemd-udevd
[ -x $UDEVD ] || UDEVD=/lib/systemd/systemd-udevd
$UDEVD --daemon
udevadm trigger --type=subsystems --action=add
udevadm trigger --type=devices --action=add
udevadm settle --timeout=10
sleep 1

mkdir -p /root/results
ls -la /dev/input > /root/results/init_input.txt 2>&1
cat /proc/bus/input/devices >> /root/results/init_input.txt 2>&1

(
  sleep 30
  uname -r > /root/results/init_memoria.txt
  ps -eo pid,rss,vsz,comm | awk 'NR==1 || $1==1 || $4=="Xorg" || $4=="retroarch" || $4=="systemd-udevd"' >> /root/results/init_memoria.txt
  free -h >> /root/results/init_memoria.txt

  {
    echo "=== POLITICAS APLICADAS POR PID 1 ==="
    echo "Scheduler sda: $(cat /sys/block/sda/queue/scheduler 2>/dev/null)"
    echo "CPU governor:  $(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor 2>/dev/null)"
    echo "Swappiness:    $(cat /proc/sys/vm/swappiness 2>/dev/null)"
    RA_PID=$(pidof retroarch)
    [ -n "$RA_PID" ] && chrt -p "$RA_PID"
  } > /root/results/init_tuning.txt 2>&1
) &

# 3. Ejecutar RetroArch con SCHED_FIFO y registrar FPS + fallos de pagina al salir
exec xinit /usr/bin/time -v -o /root/results/init_rendimiento.txt /usr/bin/chrt -f 50 "$(which retroarch)" --verbose --log-file /root/results/retroarch_fps.log --appendconfig=/usr/local/etc/ra.cfg -- /usr/bin/Xorg :0 vt1 -nolisten tcp
