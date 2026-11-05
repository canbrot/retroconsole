# Retroconsole

Proyecto de la materia de Sistemas Operativos. Tomamos una torrre vieja y lo convertiremos en una consola retro dedicada. No diseñaremos  un emulador desde 0, usando herramientas ya existentes, RetroArch con el núcleo mGBA, lo adaptaremos lo mejor posible al equipo para que corra con el mínimo de recursos y la maxima de estabilidad. El trabajo está en el sistema que hay debajo: recortar el kernel, escribir nuestro propio init en C y medir cómo se comporta el sistema operativo mientras corre el juego.

## Hardware

| Componente | Detalle |
|---|---|
| Placa madre | Biostar H61MH, Micro-ATX, BIOS legacy |
| CPU | Intel Core i3-2100, 2 núcleos y 4 hilos a 3.10 GHz |
| RAM | 4 GB DDR3 |
| Disco | HDD de 500 GB |
| Mando | USB por cable |

## Cómo arranca el sistema

De abajo hacia arriba, esto es lo que pasa al encender el equipo:

| Capa | Componente | Qué hace | Quién lo hace |
|---|---|---|---|
| 1 | BIOS | Inicia el hardware y busca dónde arrancar | Viene con la placa |
| 2 | GRUB | Carga el kernel en memoria | Ya existe en Debian |
| 3 | Kernel de Linux | Maneja CPU, memoria, disco y dispositivos | Lo recortamos y recompilamos |
| 4 | Init, PID 1 | Primer proceso del espacio de usuario, lanza todo lo demás | Lo escribimos en C |
| 5 | Servidor gráfico X | Dibuja en pantalla | Ya existe, X.Org |
| 6 | RetroArch | Ventana, video, audio y mando | Ya existe |
| 7 | Núcleo mGBA | Emula la CPU y el video de la Game Boy | Ya existe |
| 8 | ROM | El juego, un archivo .gb | Homebrew de prueba |

## Qué hacemos nosotros

**Kernel recortado.** Partimos de la configuración del kernel de Debian 13 y quitamos lo que una consola no necesita: Bluetooth, WiFi, RAID y LVM, sonido USB, CIFS y NFS. También activamos PREEMPT para tener mejor respuesta bajo carga. La idea es comparar tamaño, tiempo de arranque y uso de RAM contra el kernel original.

**Init propio en C.** Reemplaza a systemd. Monta /proc, /sys y /dev, lanza el servidor gráfico y RetroArch con fork y exec, y se queda esperando con waitpid. Si RetroArch se cae, lo vuelve a lanzar, y recoge los procesos huérfanos. También maneja las señales de apagado.

**Mediciones.** Como una Game Boy casi no gasta CPU, generamos carga artificial con stress-ng para poder medir:

- Planificación de CPU: políticas normales contra SCHED_FIFO con el sistema saturado.
- Memoria: uso por proceso, fallos de página y qué hace el OOM killer cuando se acaba la RAM.
- Disco: los planificadores bfq y mq-deadline sobre el HDD, con fio.
- Mando e interrupciones: el recorrido de una pulsación desde el USB hasta /dev/input y RetroArch.

Todo se mide en la torre real, no en la máquina virtual.

## Equipo

| Integrante | Parte |
|---|---|
| Erwin Castañeda | Init propio en C y kernel recortado, con las pruebas en el hardware real |
| random 2 | Planificación de CPU |
| random 3 | Gestión de memoria |
| random 4 | Mando, udev e interrupciones |
| random 5 | Planificación de disco |

## Estado

Trabajo en progreso. Ya está hecho: Debian 13 instalado, kernel 6.12.107 compilado con PREEMPT y recortado, y RetroArch con mGBA funcionando en la máquina virtual de desarrollo. Falta: instalar y medir el kernel nuevo en la torre, el init en C, los dos juegos y todos los experimentos. El detalle día a día está en BITACORA.md.

## Máquina virtual de desarrollo

Debian 13 sin escritorio, con git, stress-ng, fio, evtest, htop y RetroArch con mGBA.
Descarga: <ENLACE DE DRIVE>. Es un .zip, hay que descomprimirlo antes de importar.

- Usuario: canbrot
- Contraseña: <123>
- Se importa en VirtualBox con Archivo, Importar servicio virtualizado
- Configuración: 2 CPUs, entre 2 y 4 GB de RAM, EFI desactivado

La máquina virtual sirve para desarrollar y depurar. Los números finales salen de la torre.

## Estructura del repositorio

- kernel: configuración del kernel y notas de lo que se recortó
- init: código del init
- scripts: scripts de medición y de instalación
- results: salidas de las pruebas, con fecha
- informe: informe final
- docs: guías por área

## Recursos

[Carpeta Compartida en Google Drive](https://drive.google.com/drive/folders/19EVAPyKyOJJQl7c5br0oTpZVMr9Ai-22?usp=drive_link)
