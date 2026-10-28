# Concurrencia: diseño común a Linux y Windows

[Inicio](Home) · Anterior: [Bandeja de salida](Bandeja-de-salida) · Siguientes: [Linux](Concurrencia-Linux) y [Windows](Concurrencia-Windows)

Esta página define **qué** se sincroniza y con qué reglas. Las páginas de [Linux](Concurrencia-Linux) y [Windows](Concurrencia-Windows) dicen **con qué primitivas** se implementa en cada sistema.

## 1. Objetos de sincronización entre procesos

| Objeto lógico | Valor inicial | Protege o señala |
|---|---|---|
| `inEmpty[k]` | `ie` | Posiciones libres de la cola de entrada `k` |
| `inFull[k]` | 0 | Muestras disponibles en la cola de entrada `k` |
| `inMutex[k]` | libre | `head`, `tail` y `count` de la cola de entrada `k` |
| `outEmpty` | `oe` | Posiciones libres de la cola de salida |
| `outFull` | 0 | Reportes disponibles en la cola de salida |
| `outMutex` | libre | `head`, `tail` y `count` de la cola de salida |
| `idMutex` | libre | `Header.nextId` |
| `reactiveMutex` | libre | `Header.reactive[3]` (analizadores, robot y `ctrl update`) |
| `statusMutex` | libre | Tabla de estado |

Los protocolos productor–consumidor están en [Bandeja de entrada](Bandeja-de-entrada) y [Bandeja de salida](Bandeja-de-salida).

## 2. Sincronización interna de `eval-init` (entre hilos)

Estos objetos no se comparten con otros procesos. Se implementan igual en ambos sistemas con la biblioteca estándar de C++.

| Objeto | Descripción |
|---|---|
| `internalQ[3]` | Cola acotada (capacidad `q`) por tipo de muestra. Cada una tiene `std::mutex` y dos `std::condition_variable`: `notEmpty` y `notFull` |
| `refillQ` | Cola FIFO de solicitudes de recarga para el robot, con `std::mutex` y `std::condition_variable` |
| `refillDone[3]` | Una variable de condición por analizador, para avisarle que su recarga terminó |

Cada vez que un hilo cambia una muestra de lugar, actualiza la [tabla de estado](Memoria-compartida) bajo `statusMutex`.

## 3. Reglas contra interbloqueos

1. **Orden de adquisición.** Si hay que tomar varios mutex a la vez, el orden es siempre `inMutex[k]` o `outMutex` → `reactiveMutex` → `statusMutex`. Nunca al revés.
2. **Semáforo antes que mutex.** Un hilo espera primero el semáforo de espacio o de elementos y solo después toma el mutex. Nunca espera un semáforo con un mutex tomado.
3. **Secciones críticas cortas.** `statusMutex` se mantiene solo mientras se copia o se actualiza una entrada.
4. **Simulación fuera de los mutex.** Los `sleep` que simulan el tiempo de análisis o los viajes del robot se hacen siempre sin ningún mutex tomado.
5. **Un solo robot.** Las recargas se atienden en orden FIFO, así que dos analizadores no compiten por el mismo reactivo.

## 4. Esperas con tiempo límite

Todas las esperas sobre semáforos usan un **tiempo límite de 500 ms** y repiten en un ciclo, de modo que revisan `Header.shuttingDown` en cada vuelta:

- hilos de entrada (`inFull[k]`) y analizadores (`outEmpty`),
- `reg` (`inEmpty[k]`),
- `rep` (`outFull`).

Los `sleep` de simulación se parten en tramos de 100 ms para poder interrumpirlos al terminar.

## 5. Arranque: cuándo el sistema está listo

`init` crea el segmento, inicializa **todo** (cabecera, colas, tabla de estado, objetos de sincronización) y escribe `Header.magic` **al final**, con una escritura atómica de liberación. Los programas cliente (`reg`, `ctrl`, `rep`) abren el segmento, comprueban `magic` y `version`, y si no coinciden terminan con código 3. Así ningún cliente usa un sistema a medio inicializar.

## 6. Terminación ordenada

1. El hilo principal de `init` recibe la señal de terminación (Ctrl+C, SIGTERM; ver cada sistema).
2. Pone `Header.shuttingDown = 1`.
3. Los hilos salen de sus ciclos en menos de 500 ms. Los analizadores terminan el tramo de `sleep` en curso.
4. El hilo principal hace `join` de todos los hilos.
5. Libera los objetos de sincronización y el segmento (ver cada sistema).
6. Los programas cliente que detectan `shuttingDown`, o que el segmento ya no existe, terminan con un mensaje claro y código 5.

## 7. Muestras que esperan demasiado

El tiempo de espera de una muestra es `ahora - enqueueTime`. Se revisa en dos puntos: cuando el hilo de entrada la saca de su cola y cuando el analizador la saca de la cola interna. Si supera `maxWait` (`-ee`), la muestra no se analiza: se envía a la cola de salida con resultado `?` y **no consume reactivo**.
