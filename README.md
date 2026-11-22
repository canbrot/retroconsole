# Retroconsole

Proyecto de Sistemas Operativos (Universidad del Rosario). Es la opción "Linux benchmarking" del proyecto de clase: convertimos una torre vieja en una consola retro dedicada y medimos cómo Linux real resuelve los subsistemas vistos en el curso (procesos, memoria, memoria virtual, disco, E/S), para contrastarlos con lo que implementamos en UR-OS.

No diseñamos un emulador. Usamos RetroArch con el núcleo mGBA (Game Boy), y el trabajo está en el sistema que hay debajo: recortar el kernel, escribir un init propio en C y medir cómo se comporta el sistema operativo mientras corre el juego.

## Pregunta central

¿Cómo resuelve Linux real cada subsistema que vimos en clase, qué le pasa a una carga real (un emulador) y en qué se diferencia del modelo de UR-OS?

Cada área se documenta con el mismo molde: concepto de clase, cómo lo hace UR-OS, cómo lo hace Linux, evidencia medida por nosotros y límites de la medición.

## Hardware

| Componente | Detalle |
|---|---|
| Placa madre | Biostar H61MH, Micro-ATX, BIOS legacy |
| CPU | Intel Core i3-2100, 2 núcleos y 4 hilos a 3.10 GHz |
| RAM | 4 GB DDR3 (más 4 GiB de swap) |
| Disco | HDD Seagate ST3500418AS de 500 GB |
| GPU | NVIDIA GT 630 (controlador nouveau) |
| Mando | USB por cable |

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

**Kernel recortado.** Partimos de la configuración de Debian 13 y quitamos lo que una consola no necesita. Comparamos arranque y RAM contra el kernel original (promedios de 3 corridas en la torre):

| Configuración | Kernel | Arranque total | RAM al iniciar |
|---|---|---|---|
| Original con GNOME | 8,55 s | 58,94 s | 1,77 GiB |
| Recortado con GNOME | 8,29 s | 62,08 s | 1,73 GiB |
| Recortado sin GNOME | 8,32 s | 36,16 s | 0,40 GiB |

Hallazgo: recortar el kernel no aceleró el arranque. El ahorro (unos 26 s y 1,3 GiB) viene de no iniciar el escritorio. También documentamos un episodio: el initrd pesó 524 MB en vez de 106 MB hasta que usamos INSTALL_MOD_STRIP=1.

**Init propio en C.** Reemplaza a systemd. Monta /proc, /sys y /dev, lanza X y RetroArch con fork y exec, espera con waitpid, relanza a su hijo si muere (si muere 3 veces, apaga), recoge huérfanos y hace un apagado ordenado. Probado en la máquina virtual; falta correrlo en la torre.

**Memoria.** Linux no permite sustituir su gestor de memoria en el tiempo del proyecto, así que comparamos políticas aplicadas al emulador y simulamos algoritmos propios:
- Un programa en C lee /proc/meminfo y /proc/PID/status (validado contra free y ps).
- Escalones de presión con stress-ng (1G a 4G) y vmstat: el swap aparece en 4G en la torre sin escritorio, y en 1G en la VM con escritorio.
- Tres políticas con RetroArch corriendo: Linux por defecto, mlockall (bloquear el emulador en RAM) y cgroup (limitar al proceso que presiona). Ambas políticas evitan que el emulador vaya al swap; mlockall cuadruplica su RSS y el cgroup no.
- Simulador de reemplazo de páginas (FIFO, LRU, Clock) con accesos sintéticos que modelan el emulador.
- Conclusión: con RetroArch usando unos 128 MiB, la memoria no es el cuello de botella; la presión solo viene de algo ajeno al emulador.

**Disco.** Comparación de los planificadores bfq y mq-deadline sobre el HDD con fio.

**CPU.** Planificación con políticas normales contra SCHED_FIFO bajo carga.

**Mando e interrupciones.** Recorrido de una pulsación desde el USB hasta /dev/input y RetroArch.

Todas las mediciones son pocas corridas, así que las presentamos como indicios y no como pruebas. Cada cifra del informe debe salir de un archivo en results/.

## Equipo


| Integrante | Parte |
|---|---|
| Erwin Castañeda | Init propio en C y kernel recortado, con las pruebas en el hardware real |
| Joseph Doqueresana | Planificación de CPU |
| Laura Espinosa | Gestión de memoria |
| Mileidis Rojas | Mando, udev e interrupciones |
| Camilo Perdomo | Planificación de disco |

Solo Erwin tiene acceso permanente a la torre, por eso la mayoría de las mediciones las tomó él. Parte del trabajo se hizo con asistencia de una IA (Claude, de Anthropic), que propuso código y comandos; Erwin los ejecutó y verificó. Los detalles están en BITACORA.md.

## Estado

Hecho: kernel 6.12.107 recortado e instalado en la torre, mediciones de arranque, RetroArch con mGBA, init en la VM (etapas 1 a 4), experimentos de memoria en VM y torre, medición de disco.
Falta: correr el init en la torre, probar el mando, repetir las políticas de memoria para tener 3 corridas, y consolidar el informe.

## Máquina virtual de desarrollo

Debian 13 con git, stress-ng, fio, evtest, htop y RetroArch con mGBA.
Descarga: <ENLACE DE DRIVE> (es un .zip; hay que descomprimirlo antes de importar).
Usuario: canbrot. La contraseña se entrega a cada integrante por privado.

- Se importa en VirtualBox con Archivo, Importar servicio virtualizado.
- Configuración: 2 CPUs, entre 2 y 4 GB de RAM, EFI desactivado.

La VM sirve para desarrollar y depurar. Los números finales salen de la torre. Los datos de la VM se reportan solo como referencia y no son comparables con los de la torre.

## Estructura del repositorio

- kernel: configuración del kernel y notas de lo que se recortó
- init: código del init
- memoria: programas y scripts del área de memoria
- disco: scripts de medición de disco
- scripts: scripts de instalación
- results: salidas de las pruebas, con fecha
- informe: informe final
- docs: guías por área
