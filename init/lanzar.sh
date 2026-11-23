#!/bin/sh
export HOME=/root
export PATH=/usr/sbin:/usr/bin:/sbin:/bin:/usr/games
mkdir -p /run/user/0
chmod 700 /run/user/0
export XDG_RUNTIME_DIR=/run/user/0

modprobe evdev
modprobe xpad
modprobe drm
modprobe nouveau

UDEVD=/usr/lib/systemd/systemd-udevd
[ -x $UDEVD ] || UDEVD=/lib/systemd/systemd-udevd
$UDEVD --daemon
udevadm trigger --action=add
udevadm settle --timeout=10
sleep 1

echo "lanzar.sh: arrancando X y menu RetroArch"
(
  sleep 30
  mkdir -p /root/results
  uname -r > /root/results/init_memoria.txt
  ps -eo pid,rss,vsz,comm | awk 'NR==1 || $1==1 || $4=="Xorg" || $4=="retroarch" || $4=="systemd-udevd"' >> /root/results/init_memoria.txt
  free -h >> /root/results/init_memoria.txt
) &

exec xinit "$(which retroarch)" --appendconfig=/usr/local/etc/ra.cfg -- /usr/bin/Xorg :0 vt1 -nolisten tcp
