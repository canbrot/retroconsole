#!/bin/sh
export HOME=/root
export PATH=/usr/sbin:/usr/bin:/sbin:/bin:/usr/games
mkdir -p /run/user/0
chmod 700 /run/user/0
export XDG_RUNTIME_DIR=/run/user/0

modprobe ehci_pci
modprobe usbhid
modprobe evdev
modprobe joydev
modprobe xpad
modprobe drm
modprobe nouveau

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

echo "lanzar.sh: arrancando X y menu RetroArch"
(
  sleep 30
  uname -r > /root/results/init_memoria.txt
  ps -eo pid,rss,vsz,comm | awk 'NR==1 || $1==1 || $4=="Xorg" || $4=="retroarch" || $4=="systemd-udevd"' >> /root/results/init_memoria.txt
  free -h >> /root/results/init_memoria.txt
) &

exec xinit "$(which retroarch)" --appendconfig=/usr/local/etc/ra.cfg -- /usr/bin/Xorg :0 vt1 -nolisten tcp
