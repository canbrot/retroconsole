# RetroConsole

Proyecto de Sistemas Operativos de la Universidad del Rosario. En la modalidad Linux benchmarking convertimos una torre de escritorio antigua en una consola retro dedicada para medir cómo Linux real resuelve los subsistemas vistos en clase: procesos, memoria, memoria virtual, disco y E/S, y contrastarlos con el modelo de UR-OS.

No diseñamos un emulador desde cero. Usamos RetroArch 1.20.0 con el núcleo libretro-mGBA para Game Boy y centramos el trabajo en el sistema que hay debajo: recortar y compilar el kernel, escribir un init propio en C como PID 1 y medir cómo responde el sistema operativo sobre hardware real bajo carga.

## Pregunta central y enfoque

¿Cómo resuelve Linux real cada subsistema visto en clase, qué le pasa a una carga real como un emulador a 60 Hz y en qué se diferencia de UR-OS?

Cada área se documenta con el mismo molde: concepto de clase, cómo lo hace UR-OS, cómo lo hace Linux, evidencia medida por nosotros en los registros de `results/` y límites de la medición.

No era objetivo modificar el código fuente del planificador ni del gestor de memoria de Linux. Cada cambio habría obligado a recompilar y reiniciar la torre física con riesgo de dejarla sin arrancar. En su lugar, comparamos políticas que Linux ya ofrece en tiempo de ejecución y escribimos simuladores en C para evaluar los algoritmos clásicos de clase.

## Objetivos

1. Instalar Debian 13.6 en una torre vieja y compilar un kernel Linux 6.12.107 recortado.
2. Escribir un init propio en C, `myinit` como PID 1, que arranque el entorno gráfico mínimo y RetroArch sin `systemd`.
3. Correr RetroArch con mGBA usando el mínimo de recursos y manteniendo estabilidad a 60 Hz.
4. Medir arranque, memoria, CPU, disco y E/S en hardware real, y contrastar los resultados con clase y con UR-OS.
5. Documentar con honestidad qué datos salen de baterías de 3 corridas, cuáles son corridas únicas y cuáles son simulaciones sintéticas.

## Hardware

| Componente | Detalle |
| :--- | :--- |
| Placa madre | Biostar H61MH, Micro-ATX, BIOS legacy |
| CPU | Intel Core i3-2100, 2 núcleos y 4 hilos a 3,10 GHz |
| RAM y Swap | 4 GB DDR3 con 3,8 GiB útiles, más 4,0 GiB de swap en `/dev/sda5` |
| Disco | HDD Seagate ST3500418AS de 500 GB a 7200 RPM |
| GPU | NVIDIA GeForce GT 630 con controlador libre `nouveau` |
| Mando | Xbox One por cable USB, ID 045e:02d1 bajo el módulo `xpad`, compatible también con Xbox Series X\|S por USB-C |

## Cómo arranca el sistema

| Capa | Componente | Qué hace | Origen |
| :--- | :--- | :--- | :--- |
| 1 | BIOS Legacy | Inicia el hardware y busca dónde arrancar | Viene con la placa |
| 2 | GRUB | Carga la imagen del kernel y el `initrd` en memoria | Viene en Debian 13.6 |
| 3 | Kernel Linux 6.12.107 | Maneja CPU con PREEMPT, memoria con MEMCG, disco con BFQ y USB con `xpad` | Lo recortamos y recompilamos |
| 4 | `myinit` como PID 1 | Remonta `/`, monta sistemas virtuales, lanza `udev` y supervisa la sesión | Lo escribimos en C |
| 5 | Servidor gráfico Xorg | Dibuja en pantalla sin entorno de escritorio | Viene en X.Org |
| 6 | RetroArch 1.20.0 | Maneja ventana, video, audio y entrada con el driver `linuxraw` | Binario del sistema |
| 7 | Núcleo mGBA | Emula la CPU y el video de la Game Boy a 60 Hz | Librería libretro |
| 8 | ROM `.gb` | Juego homebrew legal de prueba, no incluido en el repositorio | Carga externa por USB |

## Qué hicimos y qué encontramos

### Kernel recortado y tiempos de arranque

Partimos de la configuración de Debian 13 sobre Linux 6.12.107. Activamos `CONFIG_PREEMPT=y`, `MEMCG`, `IOSCHED_BFQ=m`, `EVDEV` y `JOYSTICK_XPAD=m`, y quitamos lo que una consola no necesita: Bluetooth, Wi-Fi, RAID, LVM, audio USB, CIFS y NFS. Esto bajó el peso de los módulos en disco de 107 MB a 79 MB. Comparamos arranque y memoria en reposo con promedios de 3 corridas en la torre:

| Configuración | Fase Kernel prom. | Userspace prom. | Arranque Total prom. | RAM al iniciar |
| :--- | :---: | :---: | :---: | :---: |
| Original con GNOME, 6.12.107+deb13-amd64 | 8,55 s | 50,39 s | 58,94 s | 1,77 GiB |
| Recortado con GNOME, 6.12.107 | 8,29 s | 53,79 s | 62,08 s | 1,73 GiB |
| Recortado sin GNOME, `multi-user.target` | 8,32 s | 27,84 s | 36,16 s | 0,40 GiB o 405 MiB |

Hallazgo: recortar módulos no aceleró la fase del kernel, que se mantuvo alrededor de 8,3 s, porque los módulos que no se usan no se cargan y el piso lo marca la lectura en el disco mecánico. El ahorro real vino de quitar el escritorio y servicios como `plymouth-quit-wait` de unos 30 s y `fwupd` de unos 18 s: bajamos 22,8 s frente al original, o 25,9 s frente al recortado con GNOME, y liberamos cerca de 1,33 GiB de RAM.

Episodio documentado: al compilar sin limpiar símbolos de depuración, el `initrd` pesó 524 MB y duplicó el arranque del kernel a 16,9 s. Reinstalar módulos con `INSTALL_MOD_STRIP=1` lo bajó a 101 MB.

### Init propio en C

Reemplaza a `systemd` como PID 1. Remonta `/` en lectura y escritura con `MS_REMOUNT`, monta `/proc`, `/sys`, `/dev`, `/run` y `/tmp`, levanta `systemd-udevd` para detectar dispositivos de entrada y video, aplica ajustes del kernel como planificador `bfq`, gobernador `performance` y `swappiness=1`, automonta memorias USB con ROMs y lanza `Xorg` junto a RetroArch con `chrt -f 50` usando `fork` y `execl`. Supervisa el hijo con `waitpid`, reintentando hasta 3 veces si falla, recoge huérfanos y hace un apagado limpio con `kill`, `sync`, remontaje en solo lectura y `reboot`.

En la torre, `myinit` ocupa entre 1420 y 1468 KiB de RSS frente a 14712 KiB de `systemd`, cerca de un 90 % menos, y el sistema completo con `Xorg` y RetroArch corriendo consume 395 MiB dejando 3,4 GiB disponibles. El lanzamiento de RetroArch tarda 1,71 s con 0 fallos mayores de página. El compromiso es claro: es diez veces más ligero porque hace mucho menos, ya que no gestiona dependencias, `journald`, `logind` ni activa swap.

### Memoria

- Un programa en C lee `/proc/meminfo` y `/proc/PID/status`, validado contra `free` y `ps`.
- Escalones de presión con `stress-ng` de 1G a 4G y `vmstat`: en modo texto en la torre y en corrida única, el sistema absorbe hasta 3 GiB recortando caché de página antes de paginar; al exigir 4 GiB, el swap sube a 465 MiB y la espera de disco llega al 98 %.
- Tres políticas con RetroArch corriendo y presión externa de 5 GB en 3 corridas: Linux por defecto mandó unos 36,0 MB del emulador a swap con 9 fallos mayores en promedio; `mlockall` y `cgroups` lo evitaron en las tres corridas. `mlockall` cuadruplica el RSS del emulador hasta 536,2 MiB, mientras que `cgroups` mantiene el RSS estable en 129,6 MiB.
- Simuladores en C con datos sintéticos: en reemplazo de páginas con 48 marcos, LRU con 2848 fallos y Clock con 2962 reducen a menos de la mitad los fallos de FIFO, que registra 7510. En asignación contigua sobre 512 bloques, quedan 167 bloques libres pero una solicitud de 150 falla en First, Best y Worst Fit por fragmentación externa.
- Conclusión: con RetroArch usando unos 130 MiB, la memoria no es cuello de botella; la presión viene de procesos externos y el cgroup la contiene sin inflar la RAM.

### Disco

Comparamos `bfq` contra `mq-deadline` sobre el HDD con `fio` en 3 corridas por planificador. En lecturas aisladas secuenciales a unos 83–87 MiB/s y aleatorias 4K a unos 800 KiB/s ambos planificadores empatan, pero bajo contención de 15 s entre lectura aleatoria prioritaria y escritura secuencial de fondo de 1M:

| Métrica bajo contención de 15 s | `mq-deadline` prom. | `bfq` prom. | Efecto de `bfq` |
| :--- | :---: | :---: | :---: |
| Ancho de banda lectura del emulador | 327,3 KiB/s | 563,0 KiB/s | +72,0 % |
| Latencia media lectura del emulador | 12,23 ms | 7,22 ms | -41,0 % |
| Latencia máxima lectura del emulador | 133,6 ms | 156,1 ms | +16,8 %, picos puntuales en HDD |
| Ancho de banda escritura de fondo | 31,2 MiB/s | 14,0 MiB/s | -55,3 %, escritura sacrificada |

### CPU

Programa en C con `clock_nanosleep` que mide el retraso al despertar a 60 Hz, un fotograma cada 16,667 ms, sobre 1800 cuadros por condición en modo texto durante 3 corridas:

| Condición sobre 1800 cuadros | Latencia promedio | Latencia máxima | Cuadros tardíos > 1 ms |
| :--- | :---: | :---: | :---: |
| Normal sin carga | 89,3 µs | 110 µs | 0 |
| Normal + `stress-ng --cpu 4` | 72,7 µs | 3,90 ms | 5 – 8, promedio 6,3 |
| `nice -20` + `stress-ng --cpu 4` | 64,0 µs | 3,32 ms | 1 por corrida |
| `SCHED_FIFO 50` + `stress-ng --cpu 4` | 10,0 µs | 38 – 65 µs | 0 en las 3 corridas |

### Mando e interrupciones USB

El mando Xbox One con ID 045e:02d1 opera mediante el módulo `xpad` y no `usbhid`, exponiendo `/dev/input/event16`, o `event6` bajo `myinit`, a través de `evdev` y `/dev/input/js0` mediante `joydev`. Sin `systemd-logind`, RetroArch lo lee con el driver `linuxraw` sobre `js0`.

Las herramientas en C `evento.c` y `leer_mando.c`, basadas en `open`, `ioctl`, `select`, `read` y `close`, capturaron 46 eventos en 10 s: 12 pulsaciones, 12 liberaciones y 22 movimientos de ejes. Al inspeccionar `/proc/interrupts` durante 25 eventos de eje, los contadores APIC de los controladores USB `ehci_hcd` aumentaron en +16 para IRQ 16 en CPU0 y +45 para IRQ 23 en CPU3.

## Contraste con UR-OS

| Subsistema | Cómo lo modela UR-OS | Cómo lo resuelve Linux 6.12.107 |
| :--- | :--- | :--- |
| CPU | Colas discretas como PriorityQueue, MFQ y SJF con ticks ideales sin contención física | EEVDF y políticas de tiempo real como `SCHED_FIFO 50` con expulsión inmediata y latencias al despertar de C-states |
| Memoria | Particiones contiguas con Best-Fit y marcos fijos con FIFO, LRU y Clock | Paginación bajo demanda que recorta caché antes de tocar swap, LRU aproximado y aislamiento por `cgroups` |
| Procesos y PID 1 | El simulador controla directamente los estados READY, RUNNING y TERMINATED | Separación kernel y usuario: `myinit` gestiona montajes virtuales, `udev`, recolección de huérfanos con `waitpid` y apagado seguro |
| Disco y E/S | Operaciones bloqueantes ideales sin geometría física de disco | Interrupciones hardware APIC y arbitraje de cabezal rotacional, donde `bfq` sacrifica escritura de fondo para ganar interactividad |

## Contratiempos

- La BIOS legacy rechazó varias USB de arranque; funcionó grabar la ISO con Rufus en esquema MBR.
- El `initrd` inflado a 524 MB hizo que el primer kernel recortado arrancara el doble de lento hasta recompilar con `INSTALL_MOD_STRIP=1`.
- Sin `udev` no se creaban nodos en `/dev/input` ni `/dev/dri`; hubo que lanzar `systemd-udevd` dentro de `myinit`.
- El primer arranque autónomo falló por `/` montada en solo lectura; se resolvió con un remontaje mediante `MS_REMOUNT` en el init.
- Sin `systemd-logind` el mando no respondía con el driver `udev`; se resolvió usando `linuxraw` sobre `js0`.
- Una corrida de CPU hecha por error con GNOME abierto se descartó para no mezclar condiciones.
- Se auditaron todas las cifras contra los archivos crudos de `results/` para descartar resúmenes automáticos sin respaldo.

Todos los detalles están en `BITACORA.md`.

## Límites

Casi todas las mediciones tienen muestras pequeñas: 3 corridas en arranque, políticas de memoria, CPU y disco, y corrida única en escalones de memoria, lanzamiento de RetroArch, interrupciones USB y simuladores, por lo que son indicios consistentes y no pruebas estadísticas definitivas. Memoria y disco se midieron bajo `systemd` con swap activa en `/dev/sda5`, no bajo `myinit` donde el swap queda desactivado. Los simuladores usan trazas sintéticas y no se midieron FPS ni latencia de entrada en pantalla.

## Equipo

| Integrante | Parte y temas presentados |
| :--- | :--- |
| Erwin Castañeda | Implementación en la torre física, kernel recortado 6.12.107, init propio `myinit` en C, mediciones en hardware, repositorio y `BITACORA.md` |
| Laura Espinosa | Arquitectura general, planteamiento del problema y objetivos, análisis de planificación de disco entre `bfq` y `mq-deadline` y apoyo en entregas |
| Camilo Perdomo | Gestión de memoria con `stress-ng`, políticas `mlockall` y `cgroups`, simuladores de reemplazo y asignación contigua e informe |
| Joseph Doqueresana | Planificación de CPU a 60 Hz comparando `nice -20` y `SCHED_FIFO 50`, y cierre con conclusiones y límites |
| Mileidis Rojas | Subsistema de entrada y E/S con mando Xbox vía `xpad`, `evdev` y `joydev`, lectura de eventos en C, `/proc/interrupts` e integración en RetroArch |

Como Erwin tenía acceso físico permanente a la torre, ejecutó las compilaciones y mediciones en el hardware real mientras el equipo se repartió el análisis por subsistemas, los simuladores, el informe y la exposición. Se usó asistencia de IA con Claude y Gemini para sugerir código, comandos y auditar que cada cifra del informe coincidiera con `results/`; todos los datos salen de las ejecuciones en la torre.

## Estado

Hecho: kernel 6.12.107 recortado e instalado en la torre; baterías de 3 corridas de arranque, políticas de memoria, CPU y disco; `myinit` funcionando de punta a punta con arranque autónomo, mando activo, juego corriendo y apagado limpio; verificación de interrupciones USB; simuladores en C; informe final y guion de exposición.

Pendiente para trabajo futuro: instrumentar medición automática de FPS y latencia de entrada, completar audio bajo `myinit` y repetir las pruebas de estrés de memoria y disco directamente bajo `myinit` sin swap.

## Máquina virtual de desarrollo

Debian 13 con `git`, `stress-ng`, `fio`, `evtest`, `htop` y RetroArch con mGBA:

- Descarga: `<ENLACE DE DRIVE>`, descomprimir el `.zip` antes de importar.
- Usuario: `canbrot`, la contraseña se entrega por interno.
- Importación en VirtualBox: Archivo, Importar servicio virtualizado, con 2 CPUs, entre 2 y 4 GB de RAM y EFI desactivado.

La VM sirve para desarrollar y depurar etapas del init con instantáneas. Los números finales salen todos de la torre física y no son comparables con los de la VM.

## Estructura del repositorio

- `kernel`: configuración del kernel 6.12.107 y notas de lo que se recortó
- `init`: código en C de `myinit`, `lanzar.sh` y `ra.cfg`
- `memoria`: monitor en C de `/proc`, scripts de estrés y simuladores de paginación y fragmentación
- `cpu`: programa en C y scripts de medición de latencia a 60 Hz
- `disco`: scripts de medición con `fio` comparando `bfq` y `mq-deadline`
- `es`: programas en C `evento.c` y `leer_mando.c` e inspección de `/proc/interrupts`
- `scripts`: scripts de instalación y entorno
- `results`: salidas crudas de las pruebas con fecha
- `informe`: informe final
- `docs`: guías por área

No se suben el código fuente del kernel, las ROMs, los binarios compilados ni la VM.
