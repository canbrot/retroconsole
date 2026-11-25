# Retroconsole

Proyecto de Sistemas Operativos (Universidad del Rosario). Es la opción "Linux benchmarking" del proyecto de clase: convertimos una torre vieja en una consola retro dedicada y medimos cómo Linux real resuelve los subsistemas vistos en el curso (procesos, memoria, memoria virtual, disco, E/S), para contrastarlos con lo que implementamos en UR-OS.

No diseñamos un emulador. Usamos RetroArch 1.20.0 con el núcleo mGBA (Game Boy), y el trabajo está en el sistema que hay debajo: recortar el kernel, escribir un init propio en C y medir cómo se comporta el sistema operativo mientras corre el juego.

## Pregunta central

¿Cómo resuelve Linux real cada subsistema que vimos en clase, qué le pasa a una carga real (un emulador) y en qué se diferencia del modelo de UR-OS?

Cada área se documenta con el mismo molde: concepto de clase, cómo lo hace UR-OS, cómo lo hace Linux, evidencia medida por nosotros y límites de la medición.

## Objetivos

1. Instalar Debian 13 en una torre vieja y compilar un kernel 6.12.107 recortado.
2. Escribir un init propio en C (PID 1) que arranque la consola sin systemd.
3. Correr RetroArch con mGBA con el mínimo de recursos y la máxima estabilidad.
4. Medir arranque, memoria, CPU, disco y E/S en hardware real, y contrastar con los conceptos de clase y con UR-OS.
5. Documentar con honestidad qué se midió, cuántas veces y qué es solo una simulación.

No era objetivo modificar el gestor de memoria ni el planificador de Linux: se podría recompilar el kernel, pero cada prueba exige reiniciar y un error deja la torre sin arrancar. Se compararon políticas que Linux ya ofrece y se simularon algoritmos de clase.

## Hardware

| Componente | Detalle |
|---|---|
| Placa madre | Biostar H61MH, Micro-ATX, BIOS legacy |
| CPU | Intel Core i3-2100, 2 núcleos y 4 hilos a 3.10 GHz |
| RAM | 4 GB DDR3 (más 4 GiB de swap) |
| Disco | HDD Seagate ST3500418AS de 500 GB |
| GPU | NVIDIA GT 630 (controlador nouveau) |
| Mando | USB por cable, Microsoft X-Box One pad |

## Cómo arranca el sistema

| Capa | Componente | Qué hace | Quién lo hace |
|---|---|---|---|
| 1 | BIOS | Inicia el hardware y busca dónde arrancar | Viene con la placa |
| 2 | GRUB | Carga el kernel en memoria | Ya existe en Debian |
| 3 | Kernel de Linux 6.12.107 | Maneja CPU, memoria, disco y dispositivos | Lo recortamos y recompilamos |
| 4 | Init, PID 1 | Primer proceso de usuario, lanza todo lo demás | Lo escribimos en C |
| 5 | Servidor gráfico X | Dibuja en pantalla | Ya existe, X.Org |
| 6 | RetroArch | Ventana, video, audio y mando | Ya existe |
| 7 | Núcleo mGBA | Emula la CPU y el video de la Game Boy | Ya existe |
| 8 | ROM | El juego, un archivo .gb | Homebrew de prueba (las ROMs no se suben al repo) |

## Qué hicimos y qué encontramos

**Kernel recortado.** Partimos de la configuración de Debian 13 y quitamos lo que una consola no necesita (Bluetooth, WiFi, RAID y LVM, sonido USB, CIFS y NFS), con PREEMPT activado. Comparamos arranque y RAM contra el kernel original (promedios de 3 corridas en la torre):

| Configuración | Kernel | Arranque total | RAM al iniciar |
|---|---|---|---|
| Original con GNOME | 8,55 s | 58,94 s | 1,77 GiB |
| Recortado con GNOME | 8,29 s | 62,08 s | 1,73 GiB |
| Recortado sin GNOME | 8,32 s | 36,16 s | 0,40 GiB |

Hallazgo: recortar el kernel no aceleró el arranque. El ahorro (unos 26 s y 1,3 GiB) viene de no iniciar el escritorio. Episodio documentado: el initrd pesó 524 MB hasta que reinstalamos los módulos con `INSTALL_MOD_STRIP=1` y bajó a 101 MB.

**Init propio en C.** Reemplaza a systemd. Remonta `/` en lectura y escritura, monta `/proc`, `/sys`, `/dev`, `/run` y `/tmp`, lanza X y RetroArch con fork y exec, espera con waitpid, relanza a su hijo si muere (si muere 3 veces, apaga), recoge huérfanos y hace un apagado ordenado. En la versión final también aplica ajustes del kernel (scheduler de disco bfq, gobernador de CPU `performance`, `swappiness = 1`) y automonta memorias USB con ROMs. Se desarrolló por etapas en la máquina virtual y funciona en la torre: arranca solo desde GRUB, abre RetroArch con mando y audio y apaga limpiamente al salir. Su RSS es 1468 KiB contra 14712 KiB de systemd como PID 1, pero hace mucho menos (no gestiona dependencias, journald ni logind).

**Memoria.**
- Un programa en C lee `/proc/meminfo` y `/proc/PID/status` (validado contra `free` y `ps`).
- Escalones de presión con stress-ng (1G a 4G) y vmstat: en la torre sin escritorio el swap aparece en 4G (~465 MiB); en la VM con escritorio, ya en 1G. La comparación no es controlada.
- Tres políticas con RetroArch corriendo y presión externa de 5G (3 corridas): Linux por defecto mandó ~36 MB del emulador al swap; `mlockall` y cgroup lo evitaron en las tres corridas. `mlockall` cuadruplica el RSS del emulador (~536 MiB) y el cgroup no.
- Simuladores en C con datos sintéticos: reemplazo de páginas (FIFO, LRU, Clock) y asignación contigua (First, Best y Worst Fit).
- Conclusión: con RetroArch usando unos 128 MiB, la memoria no es el cuello de botella; la presión solo viene de algo ajeno al emulador y el cgroup lo contiene.

**Disco.** bfq contra mq-deadline sobre el HDD con fio (3 corridas por scheduler). En lecturas aisladas son equivalentes. Bajo contención, bfq entrega entre 1,5 y 1,9 veces el ancho de banda de lectura y baja la latencia promedio de 12,2 a 7,2 ms, pero la escritura de fondo cae 35 % a 66 % y la latencia máxima de lectura es peor.

**CPU.** Programa en C que mide el retraso al despertar a 60 Hz (un fotograma de 16,67 ms), 3 corridas en modo texto. Con `stress-ng --cpu 4`, el planificador normal deja 5 a 8 de 1800 fotogramas más de 1 ms tarde (máximo ~3,9 ms); `nice -20` baja a 1; `SCHED_FIFO` prioridad 50 deja 0 y un máximo de 65 µs o menos. No mide FPS ni un emulador real, y no se evaluó el riesgo de que un hilo FIFO acapare la CPU.

**Mando e interrupciones.** Programa en C que lee `struct input_event` de `/dev/input/event16` y conteo de interrupciones USB en `/proc/interrupts`. El mando usa el módulo `xpad`, no `usbhid`, y el recorte del kernel no lo rompió. Bajo el init propio, RetroArch lo lee con el driver `linuxraw` porque sin systemd-logind el driver udev no tiene permisos.

## Contratiempos

- La BIOS legacy rechazó varias USB de arranque; funcionó Rufus en modo ISO con MBR.
- El initrd inflado (524 MB) hizo que el primer kernel recortado arrancara el doble de lento.
- Sin udev no hay dispositivos de entrada: hubo que cargar `evdev` a mano.
- Primer arranque autónomo falló por `/` montada en solo lectura; se resolvió con un remontaje en el init.
- Sin systemd-logind el mando no respondía; sin PulseAudio no había sonido.
- La VM sufrió errores de disco virtual y una restauración de instantánea que borró trabajo.
- Una corrida de CPU hecha con GNOME abierto se descartó para no mezclar condiciones.
- Varias cifras de resúmenes automáticos no estaban en los archivos; por eso cada cifra debe salir de un archivo de `results/`.

Todos los detalles están en BITACORA.md.

## Límites

Casi todas las mediciones son pocas corridas (3 en arranque, políticas de memoria, CPU y disco; 1 en los escalones, el cgroup y el mando), así que son indicios y no pruebas. Memoria y disco se midieron bajo systemd con swap activa, no bajo el init propio. Los simuladores usan accesos sintéticos, no una traza real de RetroArch. No se midieron FPS.

## Equipo

| Integrante | Parte |
|---|---|
| Erwin Castañeda | Kernel recortado, init propio, gestión de memoria, planificación de disco, mediciones en la torre, repositorio y bitácora |
| Joseph Doqueresana | Planificación de CPU |
| Mileidis Rojas | Mando, udev e interrupciones |
| Laura Espinosa | Apoyo en otras entregas del curso |
| Camilo Perdomo | Apoyo en otras entregas del curso e informe |

Solo Erwin tiene acceso permanente a la torre, por eso la mayoría del código práctico y de las mediciones las hizo él. Parte del trabajo se hizo con asistencia de una IA (Claude, de Anthropic), que propuso código y comandos; Erwin los ejecutó y verificó. El curso permite el uso de IA. Los detalles están en BITACORA.md.

## Estado

Hecho: kernel 6.12.107 recortado e instalado en la torre, mediciones de arranque, RetroArch con mGBA, init funcionando en la torre, experimentos de memoria, CPU y disco con 3 corridas, lectura del mando.
Falta: probar las señales de apagado del init y capturar el apagado, probar el mando dentro de RetroArch con medición, y consolidar el informe y la exposición.

## Máquina virtual de desarrollo

Debian 13 con git, stress-ng, fio, evtest, htop y RetroArch con mGBA.
Descarga: <ENLACE DE DRIVE> (es un .zip; hay que descomprimirlo antes de importar).
Usuario: canbrot. La contraseña se entrega a cada integrante por privado.

- Se importa en VirtualBox con Archivo, Importar servicio virtualizado.
- Configuración: 2 CPUs, entre 2 y 4 GB de RAM, EFI desactivado.

La VM sirve para desarrollar y depurar. Los números finales salen de la torre. Los datos de la VM se reportan solo como referencia y no son comparables con los de la torre.

## Estructura del repositorio

- kernel: configuración del kernel y notas de lo que se recortó
- init: código del init, `lanzar.sh` y `ra.cfg`
- memoria: programas y scripts del área de memoria
- cpu: programa y script de medición de CPU
- disco: scripts de medición de disco
- es: programas de lectura del mando
- scripts: scripts de instalación
- results: salidas de las pruebas, con fecha
- informe: informe final
- docs: guías por área

No se suben el código fuente del kernel, las ROMs, los binarios compilados ni la VM.
