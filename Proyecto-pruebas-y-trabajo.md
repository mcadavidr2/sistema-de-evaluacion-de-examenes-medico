# Proyecto, pruebas y reparto de trabajo

[Inicio](Home) · Anterior: [Mecanismo de los comandos](Mecanismo-de-los-comandos)

## 1. Jerarquía del proyecto

```
evaluador/
├── Makefile
├── Readme.md        # bibliotecas, herramientas, integrantes
├── Install.md       # instalación en un directorio distinto al local
├── src/
│   ├── main_evaluator.cpp      # wrapper
│   ├── init/        # eval-init: hilos de entrada, analizadores, robot
│   ├── reg/         # eval-reg
│   ├── ctrl/        # eval-ctrl
│   ├── rep/         # eval-rep
│   ├── common/      # estructuras de memoria compartida, colas, lectura de registros
│   └── platform/
│       ├── linux/   # Mateo Cadavid
│       └── windows/ # Sebastian Vasquez
├── bin/             # binarios (no se suben al repositorio)
├── doc/             # documentación de diseño en Markdown
└── examples/        # archivos de muestras (.txt) y configuraciones
```

La documentación de diseño está en esta wiki. Una copia en Markdown va en `doc/` del repositorio, como pide el enunciado.

## 2. Interfaz de la capa `platform/`

Las dos personas implementan **la misma interfaz**, cada una para su sistema. Así el código de `common/`, `init/`, `reg/`, `ctrl/` y `rep/` no tiene `#ifdef`.

| Clase | Operaciones |
|---|---|
| `ShmSegment` | `create(nombre, tamaño)`, `open(nombre)`, `data()`, `close()`, `destroy()` |
| `Sem` | `init(valor)`, `wait()`, `timedWait(ms)`, `post()` |
| `IpcMutex` | `init()`, `lock()`, `unlock()` |
| `Terminator` | `install(callback)`, `waitForSignal()` |
| `Launcher` | `run(programa, argumentos)` para el wrapper |

Detalle de cada una: [Concurrencia en Linux](Concurrencia-Linux) y [Concurrencia en Windows](Concurrencia-Windows).

## 3. Reparto de trabajo

| Integrante | Responsabilidad |
|---|---|
| **Mateo Cadavid** (Linux) | `platform/linux/`, `Makefile` para Linux, pruebas en Linux, sección Linux de `Install.md`. Programas `init` (hilos de entrada, analizadores y robot) y `ctrl` |
| **Sebastian Vasquez** (Windows) | `platform/windows/`, `Makefile` para Windows, pruebas en Windows, sección Windows de `Install.md`. Programas `reg`, `rep` y el wrapper |
| Ambos | `common/` (estructuras y colas), `Readme.md`, revisión cruzada del código y la sustentación |

Este reparto de programas entre los dos es una propuesta. Lo único fijo es que cada uno tiene a su cargo un sistema operativo.

## 4. Plan de pruebas

Cada integrante ejecuta todas las pruebas en su sistema. Con `EVAL_TIME_SCALE=10` los tiempos simulados se dividen entre 10.

| # | Prueba | Qué debe pasar |
|---|---|---|
| 1 | Registro básico: `reg -` con líneas válidas e inválidas | Ids únicos; las líneas inválidas se informan con su número y se omiten |
| 2 | Concurrencia en la entrada: 3 instancias de `reg` llenando una cola con `-ie 2` | `reg` se bloquea con la cola llena y reanuda cuando se libera espacio |
| 3 | Salida llena: `-oe 2` sin ejecutar `rep` | Los analizadores se bloquean y reanudan al consumir con `rep` |
| 4 | Reactivo: `-b 5` con muestras grandes | El analizador pide recarga al robot, se bloquea y `list reactive` muestra los niveles |
| 5 | `ctrl update` con el analizador esperando reactivo | El analizador continúa después del `update` |
| 6 | Tiempo máximo de espera: `-ee 5` con muchas muestras de piel | Las muestras que esperan de más salen con `?` |
| 7 | `rep -i` y `rep -m` | `-i` termina al vencer el tiempo; `-m` espera hasta completar la cantidad |
| 8 | Terminación: Ctrl+C en `init` con `reg` y `rep` activos | `init` termina sin recursos huérfanos; los clientes salen con código 5 |
| 9 | Segmento previo: ejecutar `init` dos veces | La segunda falla con código 4 |
| 10 | Matar `init` con `kill -9` (Linux) o desde el Administrador de tareas (Windows) y volver a ejecutar | Linux limpia el segmento huérfano; Windows no deja residuos |
| 11 | Varias instancias de `rep` a la vez | Cada reporte lo recibe una sola instancia |
| 12 | `ctrl list all` mientras el sistema trabaja | Los cuatro listados se muestran sin bloquear el sistema |

## 5. Puntos por confirmar con el docente

1. **Opción `-ee`.** Proponemos que sea el tiempo máximo de espera de una muestra, en segundos.
2. **Consumo de reactivo.** Proponemos que una muestra de cantidad `qty` consuma `qty` unidades de su reactivo.
3. **Robot.** Proponemos capacidad de 50 unidades por viaje, almacén ilimitado y tiempos de 2 s al almacén, 2 s al analizador y 1 s de carga.
4. **Opción `-s`.** Mantenemos los dos significados que tiene en la sinopsis (reactivo de piel en `init`, nombre del segmento en `ctrl` y `rep`).
5. **Lenguaje.** C++17 en ambos sistemas.

## 6. Fechas

| Entrega | Fecha | Peso |
|---|---|---|
| Primera: diseño completo | 26 de octubre | 10 % |
| Segunda: implementación completa y sustentación | 9 de noviembre | 20 % |

Las entregas se hacen en el repositorio de GitHub. Los integrantes son los dueños y el profesor es invitado.
