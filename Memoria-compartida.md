# Memoria compartida

[Inicio](Home) · Anterior: [Programas y línea de comandos](Programas-y-linea-de-comandos) · Siguiente: [Bandeja de entrada](Bandeja-de-entrada)

`init` crea **un único segmento** de memoria compartida. Todos los procesos (`reg`, `ctrl`, `rep`) lo abren por su nombre (`-n`, `-s` o `EVAL_SHM_NAME`). La disposición es idéntica en Linux y en Windows. Solo cambia cómo se crean los objetos de sincronización (ver [Concurrencia en Linux](Concurrencia-Linux) y [Concurrencia en Windows](Concurrencia-Windows)).

## 1. Distribución del segmento

```
┌──────────────────────────────┐
│ Header                       │
├──────────────────────────────┤
│ InQueue[0]  ctl + ie Slots   │
│ InQueue[1]  ctl + ie Slots   │
│ ...                          │
│ InQueue[i-1] ctl + ie Slots  │
├──────────────────────────────┤
│ OutQueue    ctl + oe Slots   │
├──────────────────────────────┤
│ Tabla de estado              │
│   i entradas        (held)   │
│   3·q entradas      (internal│
│   3 entradas        (proc)   │
└──────────────────────────────┘
```

Tamaño total:

```
tamaño = sizeof(Header)
       + i * ( sizeof(InQueueCtl) + ie * sizeof(Slot) )
       + sizeof(OutQueueCtl) + oe * sizeof(Slot)
       + (i + 3*q + 3) * sizeof(StatusEntry)
```

## 2. Estructuras

```cpp
enum SampleType : char { BLOOD='B', DETRITUS='D', SKIN='S' };
enum Result     : char { POSITIVE='P', NEGATIVE='N', INCONCLUSIVE='?' };
enum State      : uint8_t { FREE, HELD, IN_INTERNAL, PROCESSING };

struct Slot {                  // una posición de una cola (entrada o salida)
    uint64_t id;               // identificador único de la muestra
    uint8_t  inQueue;          // cola de entrada de origen (0..i-1)
    char     type;             // 'B' | 'D' | 'S'
    uint8_t  qty;              // cantidad de muestra (1..5)
    char     result;           // 'P' | 'N' | '?'  (solo en la cola de salida)
    int64_t  enqueueTime;      // milisegundos desde epoch al registrar (límite de espera)
};

struct Header {
    uint32_t magic, version;                          // se escribe magic al final de init
    uint32_t numIn, inSize, outSize, internalSize;    // i, ie, oe, q
    uint32_t maxWait;                                 // -ee, en segundos
    int32_t  evaluatorPid;                            // pid de init (diagnóstico)
    uint8_t  shuttingDown;                            // 1 = el sistema está terminando
    uint64_t nextId;                                  // contador de identificadores
    int32_t  reactive[3];                             // niveles actuales B, D, S
    int32_t  reactiveMax[3];                          // máximos (-b, -d, -s)
};

struct InQueueCtl  { uint32_t head, tail, count;  PlatformSem empty, full;  PlatformMutex mutex; };
struct OutQueueCtl { uint32_t head, tail, count;  PlatformSem empty, full;  PlatformMutex mutex; };

struct StatusEntry {           // lo que ctrl necesita ver de la memoria privada de init
    State    state;
    uint64_t id;
    uint8_t  inQueue;
    char     type;
    uint8_t  qty;
    uint8_t  procTime;         // tiempo de procesamiento asignado, en segundos
    int64_t  enqueueTime;
};
```

`PlatformSem` y `PlatformMutex` son tipos definidos en `platform/`:

| Sistema | `PlatformSem` y `PlatformMutex` |
|---|---|
| Linux | `sem_t` y `pthread_mutex_t`, que viven dentro del segmento |
| Windows | Estructura vacía. Los objetos son objetos del kernel con nombre derivado del nombre del segmento |

Los objetos globales (`idMutex`, `reactiveMutex`, `statusMutex`) se declaran también como `PlatformMutex` dentro del `Header`.

## 3. Colas circulares

Las colas de entrada y la de salida son **buffers circulares de capacidad fija**:

- `head`: posición del próximo elemento a sacar.
- `tail`: posición donde se insertará el próximo elemento.
- `count`: elementos presentes.
- Avance: `tail = (tail + 1) % capacidad` y `head = (head + 1) % capacidad`.

El detalle de cada cola está en [Bandeja de entrada](Bandeja-de-entrada) y [Bandeja de salida](Bandeja-de-salida).

## 4. Tabla de estado

Las colas internas (una por tipo) y las muestras en análisis viven en la memoria privada de `eval-init`, pero `ctrl` es otro proceso y debe poder listarlas. Por eso `init` mantiene en el segmento una **tabla de estado** dividida en tres zonas:

| Zona | Entradas | Qué refleja |
|---|---|---|
| `held` | `i` | Muestra que un hilo de entrada ya sacó de su cola y todavía no ha podido insertar en la cola interna (estado `HELD`) |
| `internal` | `3·q` (`q` por tipo) | Muestras en las colas internas (estado `IN_INTERNAL`) |
| `proc` | 3 | Muestra que cada analizador está procesando (estado `PROCESSING`) |

Reglas:

- Cada hilo escribe solo en su zona y toma un lugar `FREE` bajo `statusMutex`.
- Al cambiar una muestra de zona, libera el lugar anterior (`FREE`).
- Las muestras que están todavía en las colas de entrada y las ya reportadas **no** usan la tabla. `ctrl` las lee directamente de sus colas.

## 5. Identificadores

`nextId` empieza en 1. Se incrementa bajo `idMutex`, así que los identificadores son únicos aunque haya varias instancias de `reg` registrando a la vez.
