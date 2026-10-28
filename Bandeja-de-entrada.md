# Bandeja de entrada

[Inicio](Home) · Anterior: [Memoria compartida](Memoria-compartida) · Siguiente: [Bandeja de salida](Bandeja-de-salida)

La bandeja de entrada está formada por **`i` colas** (opción `-i`). Cada una tiene `ie` posiciones (opción `-ie`). Las llena `reg` y las vacía un hilo de `eval-init` dedicado a cada cola.

## 1. Formato de la cola

Cada cola es un buffer circular de `ie` elementos `Slot` (ver [Memoria compartida](Memoria-compartida)), precedido de su estructura de control `InQueueCtl`:

| Campo | Función |
|---|---|
| `head` | Posición del próximo elemento a sacar |
| `tail` | Posición donde se insertará el próximo elemento |
| `count` | Elementos presentes |
| `empty` (semáforo) | Posiciones libres. Inicia en `ie` |
| `full` (semáforo) | Elementos disponibles. Inicia en 0 |
| `mutex` | Exclusión mutua sobre `head`, `tail` y `count` |

Al insertar, `reg` completa los campos del `Slot`: `id` (tomado con `idMutex`), `inQueue` (el número de cola), `type`, `qty` y `enqueueTime` (milisegundos desde epoch). El campo `result` queda sin uso en esta bandeja.

## 2. Formato de un registro de entrada

Cada línea que lee `reg` (de un archivo o de la entrada estándar) tiene la forma:

```
<cola> <tipo> <cantidad>
```

| Campo | Valores válidos |
|---|---|
| `<cola>` | Entero `0 ≤ cola ≤ i-1` |
| `<tipo>` | `B`, `D` o `S` |
| `<cantidad>` | Entero `1 ≤ cantidad ≤ 5` |

Las líneas vacías se ignoran. Las líneas con final `CRLF` se aceptan en ambos sistemas (se elimina el `\r`). Una línea inválida se informa por `stderr` con su número de línea y se omite. Ver [Mecanismo de los comandos](Mecanismo-de-los-comandos).

## 3. Variables que ayudan a manejar la cola

| Variable | Valor inicial | Quién la usa |
|---|---|---|
| `inEmpty[k]` | `ie` | `reg` espera aquí si la cola está llena |
| `inFull[k]` | 0 | El hilo de entrada `k` espera aquí si la cola está vacía |
| `inMutex[k]` | libre | Todos los que tocan `head`, `tail` o `count` de la cola `k` |
| `Header.shuttingDown` | 0 | Todos, para detectar que el sistema termina |

## 4. Protocolo

**Productor (`reg`, cola `k`)**

```
esperar(inEmpty[k])                      // se bloquea si la cola está llena
bloquear(inMutex[k])
    slot[tail] = muestra
    tail = (tail + 1) % ie ;  count++
desbloquear(inMutex[k])
avisar(inFull[k])
```

**Consumidor (hilo de entrada `k`)**

```
esperar(inFull[k]) con tiempo límite     // revisa shuttingDown al vencer el límite
bloquear(inMutex[k])
    muestra = slot[head]
    head = (head + 1) % ie ;  count--
desbloquear(inMutex[k])
avisar(inEmpty[k])
```

Reglas de este protocolo:

- Se espera primero el semáforo y después se toma el mutex. Nunca al revés.
- Si la cola está llena, `reg` **se bloquea** hasta que haya espacio, como pide el enunciado.
- Pueden correr varias instancias de `reg` a la vez. El mutex de cada cola protege la inserción.

## 5. Qué hace el hilo de entrada con la muestra

```
mientras no shuttingDown:
    esperar inFull[k]; sacar la muestra de la cola k; avisar inEmpty[k]
    anotar la muestra en la zona held de la tabla de estado
    si ahora - enqueueTime > maxWait:
        resultado = '?'  → enviar a la cola de salida
    si no:
        insertar en internalQ[tipo]    // se bloquea si está llena
        pasar la muestra de held a internal en la tabla de estado
```

## 6. Cómo la consulta `ctrl`

`list waiting` recorre cada cola `k` bajo `inMutex[k]`, desde `head` y durante `count` elementos. Después agrega las entradas `HELD` e `IN_INTERNAL` de la tabla de estado.
