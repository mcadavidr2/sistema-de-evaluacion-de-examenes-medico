# Concurrencia en Linux

**Responsable:** Mateo Cadavid

[Inicio](Home) · Anterior: [Concurrencia común](Concurrencia-comun) · Siguiente: [Concurrencia en Windows](Concurrencia-Windows)

Esta página aplica a Linux el diseño de [Concurrencia común](Concurrencia-comun). Todo el código está en `src/platform/linux/`.

## 1. Primitivas usadas

| Concepto | Primitiva en Linux |
|---|---|
| Segmento compartido | `shm_open` + `ftruncate` + `mmap(MAP_SHARED)` |
| Semáforos entre procesos | `sem_t` dentro del segmento, inicializados con `sem_init(&s, 1, valor)` (`pshared = 1`) |
| Esperar con límite | `sem_timedwait` con un plazo absoluto en `CLOCK_REALTIME` |
| Esperar sin límite | `sem_wait` |
| Avisar | `sem_post` |
| Mutex entre procesos | `pthread_mutex_t` dentro del segmento, con atributos `PTHREAD_PROCESS_SHARED` y `PTHREAD_MUTEX_ROBUST` |
| Señal de terminación | `SIGINT` y `SIGTERM` |
| Hilos | `std::thread`, `std::mutex`, `std::condition_variable` |

## 2. Segmento

- **Nombre:** `/` + nombre del segmento (por ejemplo `/evaluator`). Aparece en `/dev/shm/evaluator`.
- **Creación en `init`:** `shm_open(nombre, O_CREAT | O_EXCL | O_RDWR, 0600)`, luego `ftruncate(tamaño)` y `mmap`.
- **Apertura en `reg`, `ctrl` y `rep`:** `shm_open(nombre, O_RDWR, 0)`, sin `O_CREAT`. Si falla con `ENOENT`, el sistema no existe y el programa termina con código 2.
- **Tamaño:** el de la fórmula de [Memoria compartida](Memoria-compartida). Los `sem_t` y `pthread_mutex_t` forman parte de `InQueueCtl`, `OutQueueCtl` y `Header`, así que ya están contados.

### Segmento huérfano

En Linux el segmento **sobrevive** si `init` muere sin limpiar. Por eso, cuando `shm_open` con `O_EXCL` falla con `EEXIST`, `init` hace lo siguiente:

1. Abre el segmento existente y lee `Header.evaluatorPid`.
2. Comprueba si el proceso vive con `kill(pid, 0)`. Está vivo si devuelve 0 o falla con `EPERM`.
3. Si está vivo: «el sistema ya existe», código 4.
4. Si no: `shm_unlink` y vuelve a crear el segmento.

## 3. Semáforos y mutex

- Se inicializan una sola vez, en `init`, antes de escribir `Header.magic`. Los clientes nunca los inicializan.
- `sem_wait` y `sem_timedwait` se repiten si devuelven `EINTR`.
- **Plazo de `sem_timedwait`:** `clock_gettime(CLOCK_REALTIME)` + 500 ms. Si devuelve `ETIMEDOUT`, se revisa `shuttingDown` y se repite.
- **Mutex robustos:** si un proceso muere con un mutex tomado, el siguiente que lo pide recibe `EOWNERDEAD`. En ese caso:
  1. recalcula o valida `head`, `tail` y `count` de la cola afectada,
  2. llama a `pthread_mutex_consistent`,
  3. sigue con el protocolo normal.
- Los mutex solo se usan entre procesos. Entre hilos de `init` se usan `std::mutex` y `std::condition_variable`.

## 4. Señales y terminación

- En el hilo principal de `init` se bloquean `SIGINT` y `SIGTERM` con `pthread_sigmask` **antes de crear los demás hilos**, de modo que todos hereden la máscara. Así la señal la recibe solo el hilo principal.
- El hilo principal espera con `sigwait`. Al recibir la señal ejecuta la [terminación ordenada](Concurrencia-comun).
- `SIGPIPE` se ignora, por si la salida de `rep` se redirige a un proceso que termina.
- `reg`, `ctrl` y `rep` también salen limpiamente con `SIGINT`: cierran con `munmap` y no tocan los semáforos.

### Limpieza al terminar `init`

```
join de todos los hilos
sem_destroy(...) de cada semáforo
pthread_mutex_destroy(...) de cada mutex
munmap(segmento)
shm_unlink(nombre)
```

## 5. Wrapper

`evaluator` usa `execv` para reemplazarse por `bin/eval-<comando>`. El código de salida es el del programa real.

## 6. Compilación

```
g++ -std=c++17 -O2 -Wall -Wextra -pthread  ...  -lrt
```

- `-pthread` para hilos y mutex.
- `-lrt` para `shm_open` en versiones antiguas de glibc.
- El `Makefile` genera `bin/evaluator`, `bin/eval-init`, `bin/eval-reg`, `bin/eval-ctrl` y `bin/eval-rep`.

## 7. Comprobaciones útiles

| Qué | Comando |
|---|---|
| ¿Existe el segmento? | `ls -l /dev/shm/` |
| ¿Quedó un segmento huérfano? | `ls /dev/shm/evaluator` después de cerrar `init` |
| ¿Hilos de `init`? | `ps -L -p <pid>` |
| Borrar un huérfano a mano | `rm /dev/shm/evaluator` |
