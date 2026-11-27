# RetroConsole — Benchmarking de Linux Real vs. UR-OS

Proyecto de **Sistemas Operativos** de la Universidad del Rosario. Bajo la modalidad *Linux benchmarking*, convertimos una torre de escritorio antigua en una consola retro dedicada para medir cómo un kernel Linux real resuelve los subsistemas vistos en clase: procesos, memoria, memoria virtual, disco y E/S, y contrastarlos con el modelo académico de **UR-OS**.

No diseñamos un emulador desde cero: utilizamos **RetroArch 1.20.0** con el núcleo **libretro-mGBA** para Game Boy y centramos la ingeniería en la pila inferior —recortar y recompilar el kernel, escribir un proceso `init` propio en C como `PID 1` y auditar el comportamiento del sistema operativo sobre hardware físico bajo carga controlada.

---

## Pregunta central y metodología

> **¿Cómo resuelve Linux real cada subsistema visto en clase, qué impacto sufre una carga real, como un emulador a 60 Hz, y en qué se diferencia del modelo determinista de UR-OS?**

Cada subsistema se documenta bajo el mismo esquema: concepto teórico, modelo en UR-OS, implementación real en Linux `6.12.107`, evidencia empírica extraída de los registros en `results/` y límites metodológicos de la medición.

> **Alcance sobre el código del kernel:** No modificamos el código fuente del planificador ni del gestor de memoria de Linux; cada ajuste habría exigido recompilar y reiniciar la torre física con riesgo de dejarla inoperativa. En su lugar, comparamos políticas nativas configurables en tiempo de ejecución y construimos simuladores en C para evaluar algoritmos clásicos de clase.

---

## Objetivos

1. Instalar Debian 13.6 en hardware legado y compilar un kernel **Linux 6.12.107** recortado para consola dedicada.
2. Implementar un proceso `init` propio en C, `myinit` como `PID 1`, capaz de levantar el entorno gráfico mínimo y RetroArch sin `systemd`.
3. Ejecutar RetroArch + mGBA con el menor consumo posible de recursos y estabilidad de cuadro a 60 Hz.
4. Medir arranque, memoria, CPU, disco y E/S en la torre física, contrastando los resultados con la teoría del curso y UR-OS.
5. Reportar con rigor qué datos provienen de baterías de 3 corridas, cuáles son corridas únicas y cuáles corresponden a simulaciones sintéticas.

---

## Hardware de referencia

| Componente | Especificación |
| :--- | :--- |
| **Placa madre** | Biostar H61MH, formato Micro-ATX con BIOS legacy |
| **CPU** | Intel Core i3-2100 de 2 núcleos físicos y 4 hilos a `3,10 GHz` |
| **RAM y Swap** | `4 GB` DDR3, con `3,8 GiB` o `3909 MiB` útiles, más `4,0 GiB` de swap en `/dev/sda5` |
| **Almacenamiento** | HDD mecánico Seagate `ST3500418AS` de `500 GB` a `7200 RPM` con `ROTA=1` |
| **GPU** | NVIDIA GeForce GT 630 con controlador libre `nouveau` |
| **Controlador** | Mando Xbox One por cable USB, identificador `045e:02d1` bajo el módulo `xpad`, compatible también con Xbox Series X\|S por USB-C |

---

## Arquitectura y cadena de arranque

| Capa | Componente | Función | Origen |
| :--- | :--- | :--- | :--- |
| **1** | BIOS Legacy | Inicializa el hardware y cede el control al sector de arranque | Firmware de la placa |
| **2** | GRUB | Carga la imagen del kernel y el `initrd` en RAM | Debian 13.6 |
| **3** | Kernel Linux `6.12.107` | Gestiona CPU con `PREEMPT`, memoria con `MEMCG`, disco con `BFQ` y USB con `xpad` | Recortado y compilado |
| **4** | `myinit` como `PID 1` | Remonta `/`, monta FS virtuales, lanza `udev` y supervisa la sesión | Escrito en C |
| **5** | Servidor gráfico `Xorg` | Provee el servidor gráfico mínimo sin entorno de escritorio | X.Org |
| **6** | RetroArch `1.20.0` | Gestiona ventana, salida de video, audio y entrada mediante `linuxraw` | Binario del sistema |
| **7** | Núcleo `mGBA` | Emula la CPU y el video de Game Boy a `60 Hz` | Librería libretro |
| **8** | ROM `.gb` | Juego *homebrew* legal de prueba, no incluido en el repositorio | Carga externa / USB |

---

## Resultados y hallazgos por subsistema

### 1. Kernel recortado `6.12.107` y tiempos de arranque
Partimos del `.config` de Debian 13 asegurando `CONFIG_PREEMPT=y`, `MEMCG`, `IOSCHED_BFQ=m`, `EVDEV` y `JOYSTICK_XPAD=m`, y eliminando subsistemas innecesarios como Bluetooth, Wi-Fi, RAID, LVM, audio USB, CIFS y NFS, lo que redujo los módulos en disco de `107 MB` a `79 MB`. Promedios sobre **3 corridas** en la torre física:

| Configuración | Fase Kernel prom. | Userspace prom. | Arranque Total prom. | RAM usada al iniciar |
| :--- | :---: | :---: | :---: | :---: |
| **Original + GNOME** — `6.12.107+deb13-amd64` | `8,55 s` | `50,39 s` | `58,94 s` | `1,77 GiB` |
| **Recortado + GNOME** — `6.12.107` | `8,29 s` | `53,79 s` | `62,08 s` | `1,73 GiB` |
| **Recortado sin GNOME** — `multi-user.target` | `8,32 s` | `27,84 s` | `36,16 s` | `0,40 GiB` o `405 MiB` |

- **Hallazgo principal:** Recortar módulos no aceleró la fase del kernel, que se mantuvo en torno a `8,3 s`, pues los módulos no utilizados no se cargan y el piso lo fija la lectura en el disco mecánico. El ahorro real —`22,8 s` o `38,7 %` menos frente al original, y `25,9 s` o `41,8 %` menos frente al recortado con GNOME, junto con `~1,33 GiB` de RAM liberados— provino de eliminar el entorno gráfico y servicios como `plymouth-quit-wait` de `~30 s` y `fwupd` de `~18 s`.
- **Lección de compilación:** Al compilar sin limpiar símbolos de depuración, el `initrd` creció a `524 MB` y duplicó el tiempo de kernel a `16,9 s`; reinstalar módulos con `INSTALL_MOD_STRIP=1` lo redujo a `101 MB`.

### 2. Proceso `init` propio en C: `myinit`
Reemplaza a `systemd` como `PID 1`. Ejecuta el remontaje de `/` en lectura y escritura mediante `MS_REMOUNT`, monta `/proc`, `/sys`, `/dev`, `/run` y `/tmp`, levanta `systemd-udevd` para enumerar dispositivos de entrada y video, aplica ajustes directos —planificador `bfq`, gobernador `performance` y `swappiness=1`—, automonta memorias USB con ROMs y lanza `Xorg` junto a RetroArch con `chrt -f 50` mediante `fork` y `execl`. Supervisa el hijo con `waitpid`, reintentando hasta 3 veces si falla, recolecta huérfanos y ejecuta un apagado limpio con `kill`, `sync`, remontaje en solo lectura y `reboot`.

- **Huella de memoria:** `myinit` ocupa `1420 – 1468 KiB` de RSS frente a `14712 KiB` de `systemd`, cerca de un `90 %` menos. El sistema completo con `Xorg` y RetroArch corriendo consume `395 MiB`, dejando `3,4 GiB` disponibles.
- **Perfilado de lanzamiento:** Arranque de RetroArch en `1,71 s` con `0` fallos mayores de página.
- **Compromiso:** Es 10 veces más ligero porque omite gestión de dependencias, `journald`, `logind` y activación de swap, quedando `Swap: 0B`.

### 3. Gestión de memoria
- **Monitorización y escalones de presión con `stress-ng`:** Un programa en C consulta `/proc/meminfo` y `/proc/PID/status`. En modo texto en la torre y en corrida única, el sistema absorbe hasta `3 GiB` recortando caché de página antes de paginar; al exigir `4 GiB`, el swap sube a `465 MiB` y la espera de disco `wa` alcanza `98 %`.
- **Políticas de protección bajo presión externa de `5 GB` en 3 corridas con RetroArch abierto:**
  - *Política base:* Linux envía `~36,0 MB` —`35995 KiB`— del emulador a swap, causando `9` fallos mayores en promedio.
  - *`mlockall`:* Evita el swap dejando `0 kB`, pero cuadruplica el RSS del emulador hasta `536,2 MiB`.
  - *`cgroups` con `MemoryMax`:* Limita al proceso estresor, mantiene el swap en `0 kB` y conserva el RSS estable en `129,6 MiB`.
- **Simuladores en C con datos sintéticos:** En reemplazo de páginas con `48` marcos, `LRU` con `2848` fallos y `Clock` con `2962` reducen a menos de la mitad los fallos de `FIFO`, que registra `7510`. En asignación contigua sobre `512` bloques, quedan `167` bloques libres pero una solicitud de `150` falla en *First*, *Best* y *Worst Fit* por fragmentación externa.

### 4. Planificación de disco: `bfq` vs. `mq-deadline`
Evaluación con `fio` sobre el HDD Seagate con `ROTA=1` en 3 corridas por planificador. En lecturas aisladas secuenciales a `~83–87 MiB/s` y aleatorias 4K a `~800 KiB/s` ambos planificadores empatan, pero bajo contención —`15 s` de lectura aleatoria 4K prioritaria contra escritura secuencial de fondo de `1M`—:

| Métrica bajo contención de 15 s | `mq-deadline` prom. | `bfq` prom. | Efecto de `bfq` |
| :--- | :---: | :---: | :---: |
| **Ancho de banda lectura del emulador** | `327,3 KiB/s` | `563,0 KiB/s` | **`+72,0 %`** |
| **Latencia media lectura del emulador** | `12,23 ms` | `7,22 ms` | **`-41,0 %`** |
| **Latencia máxima lectura del emulador** | `133,6 ms` | `156,1 ms` | `+16,8 %`, picos puntuales en HDD |
| **Ancho de banda escritura de fondo** | `31,2 MiB/s` | `14,0 MiB/s` | **`-55,3 %`**, escritura sacrificada |

### 5. Planificación de CPU a `60 Hz` o `16,667 ms`
Programa en C con `clock_nanosleep` midiendo latencia de despacho sobre `1800` cuadros por condición en modo texto durante 3 corridas:

| Condición sobre `1800` cuadros | Latencia promedio | Latencia máxima | Cuadros tardíos `> 1 ms` |
| :--- | :---: | :---: | :---: |
| **Normal sin carga** | `89,3 µs` | `110 µs` | `0` |
| **Normal + `stress-ng --cpu 4`** | `72,7 µs` | `3,90 ms` | `5 – 8`, promedio `6,3` |
| **`nice -20` + `stress-ng --cpu 4`** | `64,0 µs` | `3,32 ms` | `1` por corrida |
| **`SCHED_FIFO 50` + `stress-ng --cpu 4`** | `10,0 µs` | `38 – 65 µs` | `0` en las 3 corridas |

### 6. Mando e interrupciones USB
- **Ruta del controlador:** El mando Xbox One con identificador `045e:02d1` opera mediante el módulo `xpad` y no `usbhid`, exponiendo `/dev/input/event16` —o `event6` bajo `myinit`— a través de `evdev` y `/dev/input/js0` mediante `joydev`. Sin `systemd-logind`, RetroArch lee la entrada directamente con el driver `linuxraw` sobre `js0`.
- **Captura e interrupciones:** Las herramientas en C `evento.c` y `leer_mando.c`, basadas en `open`, `ioctl`, `select`, `read` y `close`, capturaron `46` eventos en `10 s`: `12` pulsaciones, `12` liberaciones y `22` movimientos de ejes. Al inspeccionar `/proc/interrupts` durante `25` eventos de eje, los contadores APIC de los controladores USB `ehci_hcd` aumentaron en `+16` para `IRQ 16` en `CPU0` y `+45` para `IRQ 23` en `CPU3`.

---

## Contraste con UR-OS

| Subsistema | Modelo académico en UR-OS | Comportamiento real en Linux `6.12.107` |
| :--- | :--- | :--- |
| **CPU** | Colas discretas como `PriorityQueue`, `MFQ` y `SJF` con ticks ideales sin contención física | `EEVDF` y políticas de tiempo real como `SCHED_FIFO 50` con expulsión inmediata mediante `CONFIG_PREEMPT=y` y latencias por despertar de `C-states` |
| **Memoria** | Particiones contiguas con `Best-Fit` y marcos fijos con `FIFO`, `LRU` y `Clock` | Paginación bajo demanda que recorta caché antes de usar swap, LRU aproximado mediante listas activa e inactiva y aislamiento por `cgroups` |
| **Procesos y `PID 1`** | El simulador controla directamente los estados `READY`, `RUNNING` y `TERMINATED` | Separación kernel y usuario: `myinit` gestiona montajes virtuales, `udev`, recolección de huérfanos con `waitpid` y apagado seguro |
| **Disco y E/S** | Operaciones bloqueantes ideales sin geometría física de disco | Interrupciones hardware APIC y arbitraje de cabezal rotacional, donde `bfq` sacrifica escritura de fondo para garantizar lectura interactiva |

---

## Contratiempos y lecciones aprendidas

- **Arranque USB en BIOS legacy:** Varias memorias fallaron al iniciar; se resolvió grabando la ISO con Rufus en esquema MBR.
- **`initrd` inflado a `524 MB`:** Los símbolos de depuración duplicaron el tiempo de arranque del kernel hasta recompilar con `INSTALL_MOD_STRIP=1`, bajando a `101 MB`.
- **Ausencia de nodos `/dev/input` y `/dev/dri`:** Sin `systemd` fue indispensable lanzar `systemd-udevd` dentro del arranque de `myinit`.
- **Sistema de archivos raíz en solo lectura:** El primer arranque autónomo falló hasta incorporar `mount` con `MS_REMOUNT` en C.
- **Permisos sin `systemd-logind`:** RetroArch no leía el mando con el driver `udev`; se solucionó configurando `linuxraw` sobre `js0`.
- **Control de entorno en benchmarks:** Se descartó una corrida de CPU ejecutada accidentalmente con GNOME abierto para preservar la validez de las 3 corridas en modo texto.
- **Trazabilidad de logs:** Se auditaron todas las cifras contra los archivos crudos de `results/` para descartar resúmenes automáticos no respaldados.

---

## Límites metodológicos

- **Tamaño de muestra:** 3 corridas por configuración en arranque, políticas de memoria, CPU y disco; corrida única en escalones de memoria, perfilado de lanzamiento, interrupciones USB y simuladores. Son indicios empíricos consistentes, no pruebas estadísticas exhaustivas.
- **Condiciones de prueba:** Los experimentos de memoria y disco se ejecutaron bajo `systemd` con swap activa en `/dev/sda5`, mientras que `myinit` arranca sin `swapon` dejando `Swap: 0B`.
- **Métricas indirectas:** Se midieron latencias de planificación, fallos mayores de página y ancho de banda, sin captura directa de FPS en pantalla ni latencia de entrada punta a punta o *input lag*.
- **Datos sintéticos:** Los simuladores de reemplazo de páginas y fragmentación usan trazas sintéticas, no volcados reales de RetroArch.

---

## Equipo y distribución de roles

| Integrante | Rol en el proyecto y áreas presentadas |
| :--- | :--- |
| **Erwin Castañeda** | Implementación práctica en la torre física, kernel recortado `6.12.107`, desarrollo de `myinit` en C, mediciones en hardware, repositorio y `BITACORA.md` |
| **Laura Espinosa** | Arquitectura general del sistema, planteamiento del problema y objetivos, análisis de planificación de disco entre `bfq` y `mq-deadline` y apoyo en entregas |
| **Camilo Perdomo** | Gestión de memoria con `stress-ng`, políticas `mlockall` y `cgroups`, simuladores de reemplazo y asignación contigua y consolidación del informe |
| **Joseph Doqueresana** | Planificación de CPU a `60 Hz` comparando `nice -20` y `SCHED_FIFO 50`, y síntesis de conclusiones y límites metodológicos |
| **Mileidis Rojas** | Subsistema de entrada y E/S con mando Xbox vía `xpad`, `evdev` y `joydev`, lectura de eventos en C, `/proc/interrupts` e integración en RetroArch |

> **Nota de ejecución y herramientas:** Dado que Erwin contaba con acceso físico permanente a la torre, ejecutó las compilaciones y baterías de medición sobre el hardware real, mientras el equipo dividió el análisis por subsistemas, el desarrollo de simuladores, el informe y la defensa oral. Durante el desarrollo se utilizó asistencia de IA mediante Claude y Gemini para sugerir fragmentos de código, comandos y auditar la correspondencia entre el informe y los archivos crudos de `results/`; todas las métricas reportadas provienen directamente de las salidas ejecutadas en la torre.

---

## Estado del proyecto

- **Completado:** Kernel `6.12.107` recortado instalado; baterías de 3 corridas de arranque, políticas de memoria, CPU y disco; `myinit` funcionando de punta a punta en la torre con arranque autónomo, mando activo, juego en ejecución y apagado limpio; verificación de interrupciones USB; simuladores en C; informe final y guion de exposición.
- **Trabajo futuro:** Instrumentar medición automática de FPS y latencia de entrada *input lag*, habilitar salida de audio completa bajo `myinit` y repetir las pruebas de estrés de memoria y disco directamente bajo `myinit` sin swap.

---

## Máquina virtual de desarrollo

Imagen de referencia con Debian 13, `git`, `stress-ng`, `fio`, `evtest`, `htop` y RetroArch + mGBA:

- **Descarga:** `<ENLACE DE DRIVE>`, descomprimir el `.zip` antes de importar.
- **Credenciales:** Usuario `canbrot`, contraseña compartida por interno.
- **Importación en VirtualBox:** *Archivo → Importar servicio virtualizado* con `2` vCPUs, `2–4 GB` de RAM y EFI desactivado.

> **Advertencia:** La VM se utilizó únicamente para desarrollar y depurar etapas de `myinit` mediante instantáneas. Todos los resultados del proyecto provienen exclusivamente de la torre física y no son comparables con la VM.

---

## Estructura del repositorio

```text
├── kernel/    # Configuración .config del kernel 6.12.107 y notas de recorte
├── init/      # Código fuente en C de myinit, script de sesión lanzar.sh y ra.cfg
├── memoria/   # Monitor en C de /proc, scripts de estrés y simuladores de paginación y fragmentación
├── cpu/       # Medidor de latencia a 60 Hz en C y scripts de carga con stress-ng
├── disco/     # Perfiles y scripts de benchmarking con fio comparando bfq y mq-deadline
├── es/        # Herramientas en C evento.c y leer_mando.c e inspección de /proc/interrupts
├── scripts/   # Scripts auxiliares de instalación y entorno
├── results/   # Salidas crudas fechadas de todas las corridas en la torre
├── informe/   # Informe técnico final
└── docs/      # Guías técnicas por subsistema
