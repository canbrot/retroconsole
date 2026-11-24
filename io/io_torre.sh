#!/bin/bash
set -e
REPO=/home/canbrot/retroconsole
OUT=$REPO/results/io_$(date +%Y%m%d_%H%M).txt
DEV=${1:-/dev/input/event16}

[ "$(id -u)" = 0 ] || { echo "Usa sudo"; exit 1; }
modprobe evdev || true
[ -e "$DEV" ] || { echo "No existe $DEV"; exit 1; }

{
echo "E/S E INTERRUPCIONES | $(uname -r) | $(date):"
echo "--- Dispositivo USB ---"
lsusb | grep -i -E "xbox|microsoft|gamepad|joystick" || true
echo "--- /proc/bus/input/devices (mando) ---"
awk '/X-Box|Xbox/,/^$/' /proc/bus/input/devices
echo "--- /proc/interrupts (USB antes) ---"
grep -E "CPU0|ehci|xhci" /proc/interrupts
echo ""
echo ">>> PULSA VARIOS BOTONES Y MUEVE LA CRUCETA DEL MANDO AHORA <<<"
$REPO/io/leer_mando "$DEV" 25
echo ""
echo "--- /proc/interrupts (USB despues) ---"
grep -E "CPU0|ehci|xhci" /proc/interrupts
echo "FIN:"
} 2>&1 | tee "$OUT"

chown canbrot:canbrot "$OUT"
echo "Guardado en $OUT"
