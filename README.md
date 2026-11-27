# RetroConsole — Benchmarking de Linux Real vs. UR-OS

Proyecto de **Sistemas Operativos** (Universidad del Rosario). Bajo la modalidad *Linux benchmarking*, convertimos una torre de escritorio antigua en una consola retro dedicada para medir cómo un kernel Linux real resuelve los subsistemas vistos en clase (procesos, memoria, memoria virtual, disco y E/S) y contrastarlos con el modelo académico de **UR-OS**.

No diseñamos un emulador desde cero: utilizamos **RetroArch 1.20.0** con el núcleo **libretro-mGBA** (Game Boy) y centramos la ingeniería en la pila inferior —recortar y recompilar el kernel, escribir un proceso `init` propio en C (`PID 1`) y auditar el comportamiento del sistema operativo sobre hardware físico bajo carga controlada.

---

## Pregunta central y metodología

> **¿Cómo resuelve Linux real cada subsistema visto en clase, qué impacto sufre una carga real (un emulador a 60 Hz) y en qué se diferencia del modelo determinista de UR-OS?**

Cada subsistema se documenta bajo el mismo esquema: concepto teórico, modelo en UR-OS, implementación real en Linux `6.12.107`, evidencia empírica extraída de los registros en `results/` y límites metodológicos de la medición.

> **Alcance sobre el código del kernel:** No modificamos el código fuente del planificador ni del gestor de memoria de Linux; cada ajuste habría exigido recompilar y reiniciar la torre física con riesgo de dejarla inoperativa. En su lugar, comparamos políticas nativas configurables en tiempo de ejecución y construimos simuladores en C para evaluar algoritmos clásicos de clase.

---

## Objetivos

1. Instalar Debian 13.6 en hardware legado y compilar un kernel **Linux 6.12.107** recortado para consola dedicada.
2. Implementar un proceso `init` propio en C (`myinit`, `PID 1`) capaz de levantar el entorno gráfico mínimo y RetroArch sin `systemd`.
3. Ejecutar RetroArch + mGBA con el menor consumo posible de recursos y estabilidad de cuadro a 60 Hz.
4. Medir arranque, memoria, CPU, disco y E/S en la torre física, contrastando los resultados con la teoría del curso y UR-OS.
5. Reportar con rigor qué datos provienen de baterías de 3 corridas, cuáles son corridas únicas y cuáles corresponden a simulaciones sintéticas.

---

## Hardware de referencia

| Componente | Especificación |
| :--- | :--- |
| **Placa madre** | Biostar H61MH (Micro-ATX, BIOS legacy) |
| **CPU** | Intel Core i3-2100 (2 núcleos físicos, 4 hilos a `3,10 GHz`) |
| **RAM y Swap** | `4 GB` DDR3 (`3,8 GiB` / `3909 MiB` útiles) + `4,0 GiB` swap (`/dev/sda5`) |
| **Almacenamiento** | HDD mecánico Seagate `ST3500418AS` de `500 GB` (`ROTA=1`, `7200 RPM`) |
| **GPU** | NVIDIA GeForce GT 630 (controlador libre `nouveau`) |
| **Controlador** | Mando Xbox One por cable USB (`ID 045e:02d1`, módulo `xpad`; compatible también con Xbox Series X\|S por USB-C) |

---

## Arquitectura y cadena de arranque

| Capa | Componente | Función | Origen |
| :--- | :--- | :--- | :--- |
| **1** | BIOS Legacy | Inicializa el hardware y cede el control al sector de arranque | Firmware de la placa |
| **2** | GRUB | Carga la imagen del kernel y el `initrd` en RAM | Debian 13.6 |
| **3** | Kernel Linux `6.12.107` | Gestiona CPU (`PREEMPT`), memoria (`MEMCG`), disco (`BFQ`) y USB (`xpad`) | Recortado y compilado |
| **4** | `myinit` (`PID 1`) | Remonta `/`, monta FS virtuales, lanza `udev` y supervisa la sesión | Escrito en C |
| **5** | Servidor X (`Xorg`) | Provee el servidor gráfico mínimo sin entorno de escritorio | X.Org |
| **6** | RetroArch `1.20.0` | Gestiona ventana, salida de video, audio y entrada (`linuxraw`) | Binario del sistema |
| **7** | Núcleo `mGBA` | Emula la CPU y el video de Game Boy a `60 Hz` | Librería libretro |
| **8** | ROM (`.gb`) | Juego *homebrew* legal de prueba (no incluido en el repositorio) | Carga externa / USB |

---

## Resultados y hallazgos por subsistema

### 1. Kernel recortado (`6.12.107`) y tiempos de arranque
Partimos del `.config` de Debian 13 asegurando `CONFIG_PREEMPT=y`, `MEMCG`, `IOSCHED_BFQ=m`, `EVDEV` y `JOYSTICK_XPAD=m`, y eliminando subsistemas innecesarios (Bluetooth, Wi-Fi, RAID, LVM, audio USB, CIFS y NFS), lo que redujo los módulos en disco de `107 MB` a `79 MB`. Promedios sobre **3 corridas** en la torre física:

| Configuración | Fase Kernel (prom.) | Userspace (prom.) | Arranque Total (prom.) | RAM usada al iniciar |
| :--- | :---: | :---: | :---: | :---: |
| **Original + GNOME** (`6.12.107+deb13-amd64`) | `8,55 s` | `50,39 s` | `58,94 s` | `1,77 GiB` |
| **Recortado + GNOME** (`6.12.107`) | `8,29 s` | `53,79 s` | `62,08 s` | `1,73 GiB` |
| **Recortado sin GNOME** (`multi-user.target`) | `8,32 s` | `27,84 s` | `36,16 s` | `0,40 GiB` (`405 MiB`) |

- **Hallazgo principal:** Recortar módulos no aceleró la fase del kernel (`~8,3 s` constantes), pues los módulos no utilizados no se cargan y el piso lo fija la lectura en el disco mecánico. El ahorro real (`-22,8 s` / `-38,7 %` frente al original, o `-25,9 s` / `-41,8 %` frente al recortado con GNOME, y `~1,33 GiB` de RAM liberados) provino de eliminar el entorno gráfico y servicios como `plymouth-quit-wait` (`~30 s`) y `fwupd` (`~18 s`).
- **Lección de compilación:** Al compilar sin limpiar símbolos de depuración, el `initrd` creció a `524 MB` y duplicó el tiempo de kernel (`16,9 s`); reinstalar módulos con `INSTALL_MOD_STRIP=1` lo redujo a `101 MB`.

### 2. Proceso `init` propio en C (`myinit`)
Reemplaza a `systemd` como `PID 1`. Ejecuta el remontaje de `/` en lectura/escritura (`MS_REMOUNT`), monta `/proc`, `/sys`, `/dev`, `/run` y `/tmp`, levanta `systemd-udevd` para enumerar dispositivos de entrada y video, aplica ajustes directos (`bfq`, gobernador `performance`, `swappiness=1`), automonta memorias USB con ROMs y lanza `Xorg` + RetroArch (`chrt -f 50`) mediante `fork()` y `execl()`. Supervisa el hijo con `waitpid()` (reintentando hasta 3 veces si falla), recolecta huérfanos y ejecuta un apagado limpio (`kill`, `sync`, remontaje `ro` y `reboot`).

- **Huella de memoria:** `myinit` ocupa `1420 – 1468 KiB` de RSS frente a `14712 KiB` de `systemd` (`~90 %` menos). El sistema completo con `Xorg` y RetroArch corriendo consume `395 MiB` (`3,4 GiB` disponibles).
- **Perfilado de lanzamiento:** Arranque de RetroArch en `1,71 s` con `0` fallos mayores de página.
- **Compromiso:** Es 10 veces más ligero porque omite gestión de dependencias, `journald`, `logind` y activación de swap (`Swap: 0B`).

### 3. Gestión de memoria
- **Monitorización y escalones de presión (`stress-ng`):** Un programa en C consulta `/proc/meminfo` y `/proc/PID/status`. En modo texto en la torre (corrida única), el sistema absorbe hasta `3 GiB` recortando caché de página antes de paginar; al exigir `4 GiB`, el swap sube a `465 MiB` y la espera de disco (`wa`) alcanza `98 %`.
- **Políticas de protección bajo presión externa de `5 GB` (3 corridas con RetroArch abierto):**
  - *Política base:* Linux envía `~36,0 MB` (`35995 KiB`) del emulador a swap, causando `9` fallos mayores en promedio.
  - *`mlockall`:* Evita el swap (`0 kB`), pero cuadruplica el RSS del emulador hasta `536,2 MiB`.
  - *`cgroups` (`MemoryMax`):* Limita al proceso estresor, mantiene el swap en `0 kB` y conserva el RSS estable en `129,6 MiB`.
- **Simuladores en C (datos sintéticos):** En reemplazo de páginas (`48` marcos), `LRU` (`2848` fallos) y `Clock` (`2962`) reducen a menos de la mitad los fallos de `FIFO` (`7510`). En asignación contigua (`512` bloques), quedan `167` bloques libres pero una solicitud de `150` falla en *First*, *Best* y *Worst Fit* por fragmentación externa.

### 4. Planificación de disco (`bfq` vs. `mq-deadline`)
Evaluación con `fio` sobre el HDD Seagate (`ROTA=1`, 3 corridas por planificador). En lecturas aisladas secuenciales (`~83–87 MiB/s`) y aleatorias 4K (`~800 KiB/s`) ambos planificadores empatan, pero bajo contención (`15 s` de lectura aleatoria 4K prioritaria contra escritura secuencial de fondo de `1M`):

| Métrica bajo contención (`15 s`) | `mq-deadline` (prom.) | `bfq` (prom.) | Efecto de `bfq` |
| :--- | :---: | :---: | :---: |
| **Ancho de banda lectura (emulador)** | `327,3 KiB/s` | `563,0 KiB/s` | **`+72,0 %`** |
| **Latencia media lectura (emulador)** | `12,23 ms` | `7,22 ms` | **`-41,0 %`** |
| **Latencia máxima lectura (emulador)** | `133,6 ms` | `156,1 ms` | `+16,8 %` (picos puntuales en HDD) |
| **Ancho de banda escritura (fondo)** | `31,2 MiB/s` | `14,0 MiB/s` | **`-55,3 %`** (sacrificada) |

### 5. Planificación de CPU (`60 Hz` / `16,667 ms`)
Programa en C con `clock_nanosleep` midiendo latencia de despacho sobre `1800` cuadros por condición en modo texto (3 corridas):

| Condición (`1800` cuadros) | Latencia promedio | Latencia máxima | Cuadros tardíos (`> 1 ms`) |
| :--- | :---: | :---: | :---: |
| **Normal sin carga** | `89,3 µs` | `110 µs` | `0` |
| **Normal + `stress-ng --cpu 4`** | `72,7 µs` | `3,90 ms` | `5 – 8` (prom. `6,3`) |
| **`nice -20` + `stress-ng --cpu 4`** | `64,0 µs` | `3,32 ms` | `1` por corrida |
| **`SCHED_FIFO 50` + `stress-ng --cpu 4`** | `10,0 µs` | `38 – 65 µs` | `0` en las 3 corridas |

### 6. Mando e interrupciones USB
- **Ruta del controlador:**
