#!/bin/bash
set -e

if [ "$EUID" -ne 0 ]; then
    echo "Ejecuta con sudo: sudo bash disco/disco_bench.sh"
    exit 1
fi

fecha=$(date +%Y%m%d_%H%M)
out="/home/canbrot/retroconsole/results/disco_${fecha}.txt"
tmp="/var/tmp/fio_test"
mkdir -p "$tmp"

modprobe bfq || true

{
echo "Kernel: $(uname -r) | Fecha: $(date)"
lsblk -d -o NAME,MODEL,ROTA,SCHED /dev/sda

for sched in mq-deadline bfq; do
    echo ""
    echo "[Planificador: $sched]"
    echo "$sched" > /sys/block/sda/queue/scheduler
    echo "Activo: $(cat /sys/block/sda/queue/scheduler)"

    echo "1) Lectura secuencial (256M, bs=128k):"
    fio --name=seq --directory="$tmp" --rw=read --bs=128k \
        --size=256M --direct=1 --group_reporting \
        | grep -E "READ:|lat \(usec\)|lat \(msec\)"

    echo "2) Lectura aleatoria (64M, bs=4k):"
    fio --name=rand --directory="$tmp" --rw=randread --bs=4k \
        --size=64M --direct=1 --group_reporting \
        | grep -E "READ:|lat \(usec\)|lat \(msec\)"

    echo "3) Contencion 15s (lectura 4k prio=0 vs escritura 1M prio=7):"
    fio --directory="$tmp" --direct=1 --time_based --runtime=15 \
        --name=emulador --rw=randread --bs=4k --size=64M --prio=0 \
        --name=fondo --rw=write --bs=1M --size=256M --prio=7 \
        | grep -E "emulador:|fondo:|READ:|WRITE:|clat \(usec\)|clat \(msec\)"
done
} | tee "$out"

rm -rf "$tmp"
chown canbrot:canbrot "$out"
echo "Guardado en $out"
