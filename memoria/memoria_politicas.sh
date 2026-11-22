#!/bin/bash
# Compara 3 politicas de memoria para RetroArch bajo presion. Sin argumentos.
# Correr en modo texto con: sudo ~/retroconsole/memoria/memoria_politicas.sh
REPO=/home/canbrot/retroconsole
ROM=/home/canbrot/roms/puzzleboy_jam.gb
LIB=$REPO/memoria/mlock_preload.so
CFG=$REPO/init/ra.cfg
CORE=$(find /usr /home -name mgba_libretro.so 2>/dev/null | head -1)
OUT=$REPO/results/memoria_politicas_$(date +%Y%m%d_%H%M).txt

[ "$(id -u)" = 0 ] || { echo "Usa sudo"; exit 1; }
for f in "$CORE" "$ROM" "$LIB" "$CFG" /usr/bin/xinit /usr/bin/Xorg; do
  [ -e "$f" ] || { echo "Falta: $f"; exit 1; }
done

limpiar() {
  pkill -x stress-ng 2>/dev/null
  pkill -x xinit 2>/dev/null
  pkill -x retroarch 2>/dev/null
  pkill -x Xorg 2>/dev/null
  sleep 3
  sync
  echo 3 > /proc/sys/vm/drop_caches
  /usr/sbin/swapoff -a && /usr/sbin/swapon -a
}

prueba() {
  NOM=$1; PRE=$2; CG=$3
  echo "=== POLITICA $NOM ==="
  limpiar
  xinit /usr/bin/env LD_PRELOAD="$PRE" retroarch -L "$CORE" "$ROM" \
    --appendconfig="$CFG" -- /usr/bin/Xorg :0 vt1 > /dev/null 2>&1 &
  PID=""
  for i in $(seq 1 40); do
    PID=$(pgrep -x retroarch | head -1)
    [ -n "$PID" ] && break
    sleep 1
  done
  if [ -z "$PID" ]; then echo "RetroArch no arranco"; limpiar; return; fi
  sleep 10
  echo "Antes: $(grep -E 'VmLck|VmRSS|VmSwap' /proc/$PID/status | tr -s ' \t\n' ' ')"
  F0=$(awk '{print $12}' /proc/$PID/stat)
  O0=$(dmesg | grep -c "Killed process")
  ( while kill -0 $PID 2>/dev/null; do
      grep -E 'VmRSS|VmSwap' /proc/$PID/status | tr -s ' \t\n' ' '; echo
      sleep 1
    done ) > /tmp/muestras_$NOM.txt &
  MON=$!
  T0=$(date +%s)
  if [ -n "$CG" ]; then
    systemd-run --scope -q -p MemoryHigh=1G -p MemoryMax=1500M \
      stress-ng --vm 1 --vm-bytes 5G --vm-keep --timeout 20s 2>&1 | tail -1
  else
    stress-ng --vm 1 --vm-bytes 5G --vm-keep --timeout 20s 2>&1 | tail -1
  fi
  T1=$(date +%s)
  kill $MON 2>/dev/null
  if kill -0 $PID 2>/dev/null; then VIVO=si; else VIVO=NO; fi
  F1=$(awk '{print $12}' /proc/$PID/stat 2>/dev/null)
  SW=$(awk '{for(i=1;i<=NF;i++) if($i=="VmSwap:" && $(i+1)+0>m) m=$(i+1)+0} END{print m+0}' /tmp/muestras_$NOM.txt)
  O1=$(dmesg | grep -c "Killed process")
  echo "Resultado $NOM: VmSwap max (kB)=$SW | fallos mayores=$((F1-F0)) | duracion s=$((T1-T0)) | RetroArch vivo=$VIVO | procesos matados por OOM=$((O1-O0))"
}

{
echo "=== $(uname -r) $(date) ==="
free -m
prueba BASE "" ""
prueba MLOCK "$LIB" ""
prueba CGROUP "" 1
limpiar
echo "=== FIN ==="
} 2>&1 | tee "$OUT"
echo "Guardado en $OUT"
