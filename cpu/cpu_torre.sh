#!/bin/bash
# Jitter de despertar a 60 Hz: 4 condiciones, 30 s cada una. Sin argumentos. Usar con sudo.
REPO=/home/canbrot/retroconsole
BIN=$REPO/cpu/jitter_cpu
OUT=$REPO/results/cpu_$(date +%Y%m%d_%H%M).txt
[ "$(id -u)" = 0 ] || { echo "Usa sudo"; exit 1; }
[ -x "$BIN" ] || { echo "Falta $BIN"; exit 1; }
corrida() {
  NOM=$1; MODO=$2; CARGA=$3
  echo "=== $NOM ==="
  if [ "$CARGA" = 1 ]; then
    stress-ng --cpu 4 --timeout 60s > /dev/null 2>&1 &
    sleep 3
  fi
  $BIN $MODO 1800
  pkill -f stress-ng 2>/dev/null
  sleep 3
}
{
if systemctl is-active --quiet graphical.target; then MODO="CON GNOME (graphical.target activo)"; else MODO="MODO TEXTO (sin GNOME)"; fi
echo "=== $(uname -r) $(date) | $MODO ==="
corrida "normal SIN carga" normal 0
corrida "normal CON carga (stress-ng --cpu 4)" normal 1
corrida "nice -20 CON carga" nice 1
corrida "SCHED_FIFO 50 CON carga" fifo 1
echo "=== FIN ==="
} 2>&1 | tee "$OUT"
echo "Guardado en $OUT"
