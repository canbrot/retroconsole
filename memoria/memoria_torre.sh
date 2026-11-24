#!/bin/bash
# Experimento de memoria: escalones de stress-ng con vmstat. Sin argumentos.
# Guarda la salida en results/memoria_FECHA.txt. Correr en la torre, no en la VM.
DIR="$(cd "$(dirname "$0")/.." && pwd)/results"
mkdir -p "$DIR"
OUT="$DIR/memoria_$(date +%Y%m%d_%H%M).txt"
if [ -z "$(/usr/sbin/swapon --show --noheadings)" ]; then
  echo "No hay swap activa. Aborto." ; exit 1
fi
{
echo "$(uname -r) $(date):"
free -m
for G in 1G 2G 3G 4G; do
  echo "ESCALON $G:"
  vmstat 1 30 > /tmp/vm_$G.txt &
  stress-ng --vm 1 --vm-bytes $G --vm-keep --timeout 10s 2>&1 | tail -3
  wait
  cat /tmp/vm_$G.txt
  free -m
  sleep 5
done
echo "OOM en dmesg:"
sudo dmesg | grep -i -E "oom|killed process" | tail -10
} 2>&1 | tee "$OUT"
echo "Guardado en $OUT"
