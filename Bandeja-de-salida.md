# Bandeja de salida

[Inicio](Home) · Anterior: [Bandeja de entrada](Bandeja-de-entrada) · Siguiente: [Concurrencia común](Concurrencia-comun)

La bandeja de salida es **una sola cola** de `oe` posiciones (opción `-oe`). La llenan los tres hilos analizadores y la vacían los procesos `rep`.

## 1. Formato de la cola

Buffer circular de `oe` elementos `Slot` (ver [Memoria compartida](Memoria-compartida)), precedido de `OutQueueCtl`:

| Campo | Función |
|---|---|
| `head` | Posición del próximo reporte a sacar |
| `tail` | Posición donde se insertará el próximo reporte |
| `count` | Reportes presentes |
| `empty` (semáforo) | Posiciones libres. Inicia en `oe` |
| `full` (semáforo) | Reportes disponibles. Inicia en 0 |
| `mutex` | Exclusión mutua sobre `head`, `tail` y `count` |

El analizador copia la muestra al `Slot` y completa `result` con `P`, `N` o `?`.

## 2. Formato de un reporte

`rep` imprime una línea por reporte:

```
[id i k r]
```

| Campo | Significado |
|---|---|
| `id` | Identificador de la muestra |
| `i` | Cola de entrada de origen |
| `k` | Tipo de muestra: `B`, `D` o `S` |
| `r` | Resultado: `P` positivo, `N` negativo, `?` no concluyente |

## 3. Variables que ayudan a manejar la cola

| Variable | Valor inicial | Quién la usa |
|---|---|---|
| `outEmpty` | `oe` | Los analizadores esperan aquí si la cola está llena |
| `outFull` | 0 | `rep` espera aquí si no hay reportes |
| `outMutex` | libre | Analizadores, `rep` y `ctrl` |
| `Header.shuttingDown` | 0 | Todos |

## 4. Protocolo

**Productor (cada analizador, y los hilos de entrada cuando descartan una muestra con `?`)**

```
esperar(outEmpty)                       // se bloquea si la salida está llena
bloquear(outMutex)
    slot[tail] = reporte
    tail = (tail + 1) % oe ;  count++
desbloquear(outMutex)
avisar(outFull)
```

**Consumidor (`rep`)**

```
esperar(outFull) con tiempo límite
bloquear(outMutex)
    reporte = slot[head]
    head = (head + 1) % oe ;  count--
desbloquear(outMutex)
avisar(outEmpty)
imprimir [id i k r]
```

Reglas de este protocolo:

- No se inserta más allá de `oe`. Si la salida está llena, el productor espera.
- Puede haber varias instancias de `rep` consumiendo. El mutex serializa la extracción y cada reporte lo recibe una sola instancia.
- Mientras el analizador espera espacio en la salida no mantiene ningún mutex tomado.

## 5. Cómo la consulta `ctrl`

`list reported` toma `outMutex` y recorre `count` elementos desde `head` **sin consumirlos**. Después suelta el mutex.
