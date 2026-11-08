# Implementación: cómo quedó el código

[Inicio](Home.md)

Esta página resume cómo se implementó el diseño y dónde la implementación concreta algo que el diseño dejaba abierto.

## 1. Mapa del código

| Carpeta / archivo | Contenido |
|---|---|
| `src/platform/platform.h` | Interfaz de la capa dependiente del sistema operativo |
| `src/platform/linux/` | `shm.cpp` (shm_open/mmap), `sync.cpp` (sem_t, mutex robustos), `process.cpp` (señales, execv) |
| `src/platform/windows/` | `shm.cpp` (CreateFileMapping), `sync.cpp` (semáforos y mutex con nombre), `process.cpp` (ConsoleCtrlHandler, CreateProcess) |
| `src/common/shared.h/.cpp` | Estructuras del segmento, disposición en memoria, colas de entrada y salida (protocolo productor/consumidor) |
| `src/common/util.*` | Utilidades: enteros, variables de ambiente, reloj |
| `src/init/main.cpp` | Evaluador: hilos de entrada, analizadores, robot, tabla de estado |
| `src/reg/`, `src/rep/`, `src/ctrl/` | Los tres programas cliente |
| `src/main_evaluator.cpp` | Wrapper `evaluator` |

`common/`, `init/`, `reg/`, `rep/` y `ctrl/` no tienen `#ifdef`: todo lo que depende del sistema operativo está detrás de `platform.h`.

## 2. Decisiones de implementación

| Tema | Decisión |
|---|---|
| Nombres de objetos | `common` construye el nombre lógico (`<segmento>_inEmpty_<k>`, etc.). Linux lo ignora; Windows lo usa para el objeto del kernel y lo guarda en el segmento |
| Espera en semáforos | `Shared::acquire` espera en tramos de 500 ms y revisa `shuttingDown` en cada vuelta. En Windows, además, `WaitForMultipleObjects` incluye el evento de terminación y despierta al instante |
| Exclusión mutua | `Guard` (RAII) toma el mutex en el constructor y lo suelta en el destructor. Así cada mutex se toma y se suelta en el mismo hilo, requisito de los mutex de Windows |
| Mutex abandonado | `Lock::Recovered` (Linux `EOWNERDEAD`, Windows `WAIT_ABANDONED`): se corrigen `head`, `tail` y `count` de la cola afectada |
| Orden de bloqueo en `init` | mutex de la cola interna → `statusMutex`; `reactiveMutex` nunca se mantiene mientras se espera otro mutex |
| Tabla de estado | Una muestra pasa por `held` (hilo de entrada) → `internal` (cola interna) → `proc` (analizador). Mientras el analizador espera reactivo la muestra está en `proc` con estado `IN_INTERNAL`, de modo que `list waiting` la sigue mostrando; pasa a `PROCESSING` al empezar a analizar |
| Cliente ante un sistema muerto | Al abrir el segmento, los clientes comprueban que el pid de `init` siga vivo; si no, terminan con código 2 |

## 3. Puntos que el enunciado no define

| Tema | Decisión |
|---|---|
| `-ee` | Tiempo máximo de espera de una muestra (segundos). La que lo supera sale con `?` y no consume reactivo |
| Reactivo | Una muestra de cantidad `qty` consume `qty` unidades |
| Muestra que necesita más reactivo que el máximo del analizador | Nunca podría procesarse; sale con `?` sin consumir |
| Robot | 50 unidades por viaje (`EVAL_ROBOT_CAPACITY`); 2 s al almacén, 2 s al analizador y 1 s de carga; almacén ilimitado; un viaje lleva un solo tipo de reactivo |
| `ctrl update` | Suma al nivel con tope en el máximo. El analizador que espera revisa el nivel cada 100 ms, así que continúa en cuanto alcanza |
| Archivo `.spl` | Una línea por cada línea del archivo de origen: el id, `ERROR` si era inválida, y línea vacía si estaba vacía |
| `rep -i` | Termina con código 0 al vencer el tiempo; con código 5 si el sistema termina antes |

## 4. Limitaciones conocidas

- **Windows:** si un cliente (por ejemplo un `ctrl` esperando entrada) sigue abierto cuando termina `init`, Windows mantiene vivo el segmento hasta que ese cliente lo cierre y un `init` nuevo con el mismo nombre falla con código 4. Basta cerrar el cliente.
- **Windows:** los argumentos de la línea de comandos usan la página de códigos ANSI del sistema; los nombres de archivo con caracteres fuera de ella pueden no abrirse.
- Con `EVAL_TIME_SCALE` los tiempos simulados se dividen, pero `-ee` se mide siempre en segundos reales.
