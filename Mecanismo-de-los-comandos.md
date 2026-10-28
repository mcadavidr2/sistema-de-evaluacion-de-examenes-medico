# Mecanismo de los comandos

[Inicio](Home) · Anterior: [Concurrencia en Windows](Concurrencia-Windows) · Siguiente: [Proyecto, pruebas y reparto](Proyecto-pruebas-y-trabajo)

Los nombres de semáforos y mutex son los de [Concurrencia común](Concurrencia-comun).

## 1. `init`

1. Analiza y valida las opciones. Todos los enteros deben ser mayores que cero.
2. Intenta crear el segmento. Si ya existe y está vivo termina con código 4. En Linux, si es un residuo, lo elimina y lo vuelve a crear. Ver [Linux](Concurrencia-Linux) y [Windows](Concurrencia-Windows).
3. Calcula el tamaño (ver [Memoria compartida](Memoria-compartida)), lo crea y lo inicializa: cabecera, colas de entrada y de salida, tabla de estado y objetos de sincronización.
4. Guarda su pid en `Header.evaluatorPid` y escribe `Header.magic` al final.
5. Crea los hilos: `i` de entrada, 3 analizadores y 1 robot.
6. Se queda ocupando la consola. Si `EVAL_LOG=1` imprime los eventos. Espera la señal de terminación y ejecuta la [terminación ordenada](Concurrencia-comun).

### Hilo de entrada `k`

Ver [Bandeja de entrada](Bandeja-de-entrada), sección 5.

### Hilo analizador (uno por tipo `t`)

```
mientras no shuttingDown:
    s = internalQ[t].sacar()                      // espera si está vacía
    si ahora - s.enqueueTime > maxWait:           // espera excesiva
        resultado = '?' ; ir a SALIDA
    // reactivo
    bloquear(reactiveMutex)
    mientras reactive[t] < s.qty:
        desbloquear(reactiveMutex)
        solicitar recarga al robot (t) ; esperar refillDone[t]     // el analizador queda bloqueado
        bloquear(reactiveMutex)
    reactive[t] -= s.qty
    desbloquear(reactiveMutex)
    estado = PROCESSING ; p = aleatorio en el rango del tipo
    dormir(p / EVAL_TIME_SCALE)
    resultado = sorteo (P 50 %, N 35 %, ? 15 %)
    SALIDA: poner el reporte en la cola de salida (ver Bandeja de salida)
```

**Reglas de la simulación**

| Tipo | Tiempo de análisis (s) | Resultado | Probabilidad |
|---|---|---|---|
| B | entero uniforme 1..7 | P (positivo) | 50 % |
| D | entero uniforme 5..20 | N (negativo) | 35 % |
| S | entero uniforme 8..25 | ? (no concluyente) | 15 % |

**Consumo de reactivo (supuesto):** una muestra de cantidad `qty` consume `qty` unidades del reactivo de su tipo.

### Hilo robot

Un solo robot. Atiende las solicitudes en orden FIFO:

```
mientras no shuttingDown:
    t = refillQ.sacar()
    dormir(2 s)                              // viaje al almacén
    carga = min(EVAL_ROBOT_CAPACITY, reactiveMax[t] - reactive[t])
    dormir(2 s)                              // viaje al analizador
    dormir(1 s)                              // carga; el analizador sigue bloqueado
    bloquear(reactiveMutex) ; reactive[t] += carga ; desbloquear(reactiveMutex)
    avisar refillDone[t]
```

El almacén se asume con existencias ilimitadas. Un viaje lleva reactivo de **un solo tipo**. Si después de una recarga el analizador sigue sin reactivo suficiente, solicita otro viaje.

## 2. `reg`

1. Abre el segmento. Si no existe, termina con código 2.
2. Lee los registros de los archivos indicados o de la entrada estándar (`-`). Un archivo que no existe se ignora sin mensaje.
3. Formato de cada línea: `<cola> {B|D|S} <cantidad>` (ver [Bandeja de entrada](Bandeja-de-entrada)).
4. Para cada registro válido toma un id con `idMutex` y lo inserta con el protocolo productor. Si la cola está llena, se bloquea hasta que haya espacio.
5. **Salida:**
   - Con entrada estándar imprime el id de cada muestra aceptada, un entero por línea.
   - Con un archivo `x.ext` crea `x.spl` con una línea por cada línea del archivo de origen: el `<id>` de la muestra, o `ERROR` si la línea era inválida.
6. Pueden ejecutarse varias instancias a la vez. Los ids son únicos gracias a `idMutex`.

## 3. `ctrl`

Modo interactivo con el prompt `> `. Termina al llegar el fin de entrada (`Ctrl+D` en Linux, `Ctrl+Z` y `Enter` en Windows). Una entrada inválida muestra un mensaje de error y sigue en el modo interactivo.

### `list [processing|waiting|reported|reactive|all]`

Sin argumento equivale a `all`.

| Subcomando | Fuente | Formato |
|---|---|---|
| `processing` | Tabla de estado, estado `PROCESSING` | `Processing:` y luego `[id i k q p]` por cada muestra |
| `waiting` | Colas de entrada (bajo `inMutex[k]`) y tabla de estado, estados `HELD` e `IN_INTERNAL` | `Waiting:` y luego `[id i k q]` |
| `reported` | Cola de salida (bajo `outMutex`), sin consumirla | `Reported:` y luego `[id i k r]` |
| `reactive` | `Header.reactive[]` bajo `reactiveMutex` | `Reactive:` y luego `B <n>`, `D <n>`, `S <n>` |
| `all` | Los cuatro anteriores | En ese orden |

Significado de los campos: `id` identificador, `i` cola de entrada de origen, `k` tipo, `q` cantidad, `p` tiempo de análisis asignado en segundos, `r` resultado.

Para no tomar varios mutex a la vez, `all` ejecuta los cuatro listados uno tras otro. Cada uno es coherente por sí mismo.

### `update {B|D|S} <entero>`

1. Valida el tipo y que el entero sea positivo.
2. Bajo `reactiveMutex`, suma el entero al nivel de ese reactivo. El nivel se limita a `reactiveMax`. Si se pasa, se informa cuánto se aceptó.
3. Avisa al analizador de ese tipo, por si estaba esperando reactivo.

## 4. `rep`

1. Abre el segmento. Si no existe, termina con código 2.
2. **`-i <seg>`:** durante `<seg>` segundos consume los reportes que lleguen. Espera `outFull` con el tiempo que queda como límite. Al vencer el tiempo termina.
3. **`-m <n>`:** consume exactamente `<n>` reportes. Si todavía no hay suficientes, **espera** hasta completarlos.
4. Cada consumo sigue el protocolo consumidor de [Bandeja de salida](Bandeja-de-salida) e imprime `[id i k r]`.
5. Si el sistema termina mientras espera, imprime un mensaje y sale con código 5.
