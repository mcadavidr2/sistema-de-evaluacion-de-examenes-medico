# Guía de ejecución paso a paso

[Inicio](Home.md) · Antes de empezar, compile el proyecto: [Install.md](../Install.md)

Esta guía sirve para Linux y para Windows. Cada paso dice qué escribir y **qué debería verse**.

## 0. Cómo funciona, en cuatro líneas

- Todo se invoca con `evaluator` y un comando: `init`, `reg`, `ctrl` o `rep`.
- Cada comando corre en **su propia consola**. `init` no devuelve el control: se queda ocupando su consola mientras el sistema funciona.
- Los comandos se encuentran entre sí por el **nombre del sistema**. Por omisión es `evaluator`. Se cambia con `-n` (en `init` y `reg`), con `-s` (en `ctrl` y `rep`) o con la variable `EVAL_SHM_NAME`.
- Para pruebas conviene `EVAL_TIME_SCALE=10`: divide los tiempos simulados entre 10 (una muestra de sangre tarda 0,1 a 0,7 s en vez de 1 a 7 s).

| Qué se escribe | Linux | Windows (`cmd`) | Windows (PowerShell) |
|---|---|---|---|
| Ejecutar | `bin/evaluator init` | `bin\evaluator.exe init` | `bin\evaluator.exe init` |
| Variable de ambiente | `export EVAL_TIME_SCALE=10` | `set EVAL_TIME_SCALE=10` | `$env:EVAL_TIME_SCALE=10` |
| Fin de entrada en `ctrl` | `Ctrl+D` | `Ctrl+Z` y `Enter` | `Ctrl+Z` y `Enter` |
| Ver el código de salida | `echo $?` | `echo %errorlevel%` | `echo $LASTEXITCODE` |

En Windows se **compila** en la consola MSYS2 UCRT64, pero se **ejecuta** en `cmd` o PowerShell. La consola MSYS2 es bash: allí hay que escribir `bin/evaluator.exe` con barras normales y `export` para las variables.

## 1. Paso a paso

Abra cuatro consolas en la carpeta del proyecto.

### Consola 1: iniciar el sistema (`init`)

```
bin/evaluator init                     (Windows: bin\evaluator.exe init)
```

Debería ver:

```
sistema 'evaluator' iniciado: i=5 ie=6 oe=10 q=6 reactivo B/D/S=100/100/100 espera máxima=60 s
Ctrl+C para terminar
```

y la consola queda ocupada. Cuando lleguen muestras, `init` imprime sus eventos: `entrada 0: muestra 1 (B x2)`, `analizador B: procesa muestra 1 durante 7 s`, `reporte: muestra 1 (B x2, cola 0) -> P`, `robot: cargó 50 unidades de B`. Con `EVAL_LOG=0` no imprime eventos.

Opciones: `-i` colas de entrada (5), `-ie` posiciones por cola (6), `-oe` posiciones de la salida (10), `-n` nombre, `-b -d -s` reactivo de sangre, detritos y piel (100), `-q` tamaño de las colas internas (6), `-ee` espera máxima en segundos (60).

### Consola 2: registrar muestras (`reg`)

Un registro es una línea: `<cola> {B|D|S} <cantidad>`. La cola va de `0` a `i-1` y la cantidad de `1` a `5`.

**Con un archivo:**

```
bin/evaluator reg examples/muestras1.txt
```

No imprime nada. Crea `examples/muestras1.spl` con una línea por cada línea del archivo: el id asignado (`1`, `2`, ...) o `ERROR` si la línea era inválida. Un archivo que no existe se ignora sin mensaje.

**De forma interactiva** (la `-` indica entrada estándar):

```
bin/evaluator reg -
0 B 2          <- usted escribe
1              <- el sistema responde con el id
9 B 1          <- cola inexistente
<stdin>:2: línea inválida (la cola debe estar entre 0 y 4): 9 B 1
```

Termine con `Ctrl+D` (Linux) o `Ctrl+Z` y `Enter` (Windows). Puede haber varias instancias de `reg` a la vez: los ids no se repiten. Si una cola está llena, `reg` **espera** hasta que se libere una posición.

### Consola 3: consultar y recargar (`ctrl`)

```
bin/evaluator ctrl
> list                      equivale a list all
> list processing           muestras en análisis:      [id i k q p]
> list waiting              esperando ser tomadas:     [id i k q]
> list reported             ya reportadas sin tomar:   [id i k r]
> list reactive             niveles de reactivo:       B n / D n / S n
> update B 20               suma 20 al reactivo de sangre (con tope en el máximo)
```

`id` es el identificador, `i` la cola de entrada, `k` el tipo (B, D o S), `q` la cantidad, `p` el tiempo de análisis en segundos y `r` el resultado: `P` positivo, `N` negativo, `?` no concluyente. Termine con fin de entrada (`Ctrl+D` o `Ctrl+Z` y `Enter`). Un comando inválido muestra un mensaje de error y sigue.

### Consola 4: recoger los reportes (`rep`)

```
bin/evaluator rep -m 6       espera y muestra exactamente 6 reportes
bin/evaluator rep -i 10      muestra los reportes que lleguen durante 10 segundos
```

Cada reporte sale así: `[1 0 B P]` (id, cola, tipo, resultado). Con `-m`, si todavía no hay reportes, **espera**. Si hay varias instancias de `rep`, cada reporte lo recibe una sola.

### Terminar

`Ctrl+C` en la consola de `init`. Debería ver `terminando...` y `sistema terminado`. Los programas que estaban esperando (`rep`, `reg`) salen con un mensaje y **código 5**.

## 2. Pruebas automáticas

| Sistema | Dónde | Comando | Resultado esperado |
|---|---|---|---|
| Linux | Terminal normal | `make test` | `Resultado: 34 ok, 0 con falla` |
| Windows | Consola MSYS2 UCRT64 | `bash tests/run_tests.sh` | `Resultado: 27 ok, 0 con falla` |

En Windows son 27 porque las pruebas 8 y 10 (señales) se hacen a mano, como se describe abajo. Tardan un par de minutos.

## 3. Pruebas manuales

**Prueba 8: terminación con Ctrl+C (Windows y Linux).**
1. Consola 1: `EVAL_TIME_SCALE=10` y `bin/evaluator init`.
2. Consola 2: `bin/evaluator rep -m 50` (queda esperando).
3. En la consola 1 pulse `Ctrl+C`.
4. Esperado: `init` imprime `terminando...` y `sistema terminado`; `rep` sale solo con `el sistema terminó antes de completar los reportes (0 de 50)` y código `5`.

**Prueba 10: terminar `init` a la fuerza.**
- *Windows:* con `init` corriendo, abra el Administrador de tareas (`Ctrl+Shift+Esc`), pestaña Detalles, y finalice `eval-init.exe`. Vuelva a ejecutar `init`: debe arrancar sin error, porque Windows libera el segmento al morir el proceso.
- *Linux:* `kill -9 <pid de eval-init>`. El segmento queda en `/dev/shm/evaluator`; `ctrl` y `rep` lo detectan como inexistente (código 2). Al ejecutar `init` de nuevo, detecta el residuo, lo elimina y arranca.

**Wrapper desde otra carpeta.** Ejecute `evaluator rep -m 1` con la ruta completa desde otro directorio y sin `init` activo. Debe responder `el sistema 'evaluator' no existe` con código `2`: el wrapper encuentra los `eval-*` junto a sí mismo.

## 4. Escenarios para mostrar en la sustentación

Use `EVAL_TIME_SCALE=10` salvo que se indique otra cosa.

| Qué se quiere mostrar | Cómo | Qué se ve |
|---|---|---|
| Registro concurrente y cola llena | `init -i 1 -ie 2 -q 1 -oe 1`; lance 3 `reg -` con varias líneas `0 B 1` | Los `reg` se bloquean; al ejecutar `rep -m 12` se reanudan y terminan |
| Salida llena | `init -oe 2` sin `rep`; registre 6 muestras | `ctrl` → `list reported` muestra solo 2 y los analizadores esperan; `rep -m 6` los libera |
| Robot y reactivo | `init -b 5`; registre muestras `0 B 5` | En `init`: `solicita recarga` y `robot: cargó ...`; `ctrl` → `list reactive` |
| Recarga manual | `EVAL_TIME_SCALE=1`, `init -b 5`, registre dos `0 B 5`; mientras el analizador espera, `ctrl` → `update B 5` | El analizador continúa sin esperar al robot |
| Espera máxima | `EVAL_TIME_SCALE=4`, `init -ee 1`; registre 6 muestras `0 S 1` | Varios reportes salen con `?` sin consumir reactivo |
| Varios `rep` | Registre 20 muestras; lance dos `rep -m 10` | Cada reporte lo recibe un solo `rep` |
| Estado en vivo | Mientras hay trabajo, `ctrl` → `list all` | Cuatro bloques: `Processing:`, `Waiting:`, `Reported:`, `Reactive:` |

## 5. Códigos de salida

| Código | Significado |
|---|---|
| 0 | Éxito |
| 1 | Error de uso o de argumentos |
| 2 | El segmento no existe (no se ha ejecutado `init`) |
| 3 | Error del sistema operativo, o segmento inválido |
| 4 | El sistema ya existe (solo `init`) |
| 5 | El sistema terminó mientras el programa esperaba |

## 6. Problemas frecuentes

| Síntoma | Causa y solución |
|---|---|
| `error: el sistema 'evaluator' no existe` (código 2) | `init` no está corriendo, o usa otro nombre. Revise `-n`, `-s` y `EVAL_SHM_NAME` |
| `error: el sistema 'evaluator' ya existe` (código 4) | Ya hay un `init` con ese nombre. Ciérrelo, o use otro nombre con `-n`. En Windows también puede ser un `ctrl` abierto que mantiene vivo el segmento: ciérrelo |
| `reg` no termina | Una cola de entrada está llena. Ejecute `rep` o mire `ctrl` → `list waiting` y `list reported` |
| `rep -m N` no termina | Todavía no hay N reportes. Con tiempos sin escalar, una muestra tarda hasta 25 s. Use `EVAL_TIME_SCALE=10` |
| Windows, MSYS2: `binevaluator.exe: command not found` | En bash la barra `\` se pierde. Escriba `bin/evaluator.exe`, o ejecute desde `cmd` |
| Windows: `mingw32-make: command not found` | Está en la consola equivocada. Debe ser MSYS2 **UCRT64** |
| Windows: acentos mal escritos en `cmd` | Use la versión actual, que pone la consola en UTF-8. Si persiste: `chcp 65001` |
| Linux: segmento huérfano tras un `kill -9` | `rm /dev/shm/evaluator`, o simplemente ejecute `init` de nuevo (limpia el residuo) |
| Linux: `evaluator: not found` | Falta compilar (`make`) o se está escribiendo la ruta mal: el binario está en `bin/` |
