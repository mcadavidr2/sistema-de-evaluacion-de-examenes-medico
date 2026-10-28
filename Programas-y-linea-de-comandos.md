# Programas y línea de comandos

[Inicio](Home) · Siguiente: [Memoria compartida](Memoria-compartida)

## 1. Programas

### 1.1 Wrapper `evaluator`

Es el único programa que invoca el usuario. Lee el primer argumento (`init`, `reg`, `ctrl`, `rep`) y ejecuta el programa real correspondiente con el resto de argumentos. Devuelve el código de salida del programa real.

| Comando | Programa real (Linux / Windows) | Función |
|---|---|---|
| `evaluator init ...` | `bin/eval-init` / `bin\eval-init.exe` | Crea el sistema y ejecuta el evaluador (hilos). Ocupa la consola. |
| `evaluator reg ...` | `bin/eval-reg` / `bin\eval-reg.exe` | Registra muestras (productor). |
| `evaluator ctrl ...` | `bin/eval-ctrl` / `bin\eval-ctrl.exe` | Consola interactiva de control. |
| `evaluator rep ...` | `bin/eval-rep` / `bin\eval-rep.exe` | Consume y muestra reportes (consumidor). |

Si el comando no existe o faltan argumentos, el wrapper imprime el uso y termina con código 1.

| Sistema | Cómo lanza el programa real |
|---|---|
| Linux | `execv`: el wrapper se reemplaza por el programa real |
| Windows | `CreateProcess`, espera con `WaitForSingleObject` y devuelve el código con `GetExitCodeProcess` |

## 2. Línea de comandos

```
evaluator init [-i <int>] [-ie <int>] [-oe <int>] [-n <string>]
               [-b <int>] [-d <int>] [-s <int>] [-q <int>] [-ee <int>]
evaluator reg  [-n <string>] { <archivo> ... | - }
evaluator ctrl [-s <string>]
evaluator rep  [-s <string>] { -i <segundos> | -m <cantidad> }
```

### `init`

| Opción | Significado | Por omisión |
|---|---|---|
| `-i` | Número de colas de entrada (> 0) | 5 |
| `-ie` | Posiciones por cola de entrada | 6 |
| `-oe` | Posiciones de la cola de salida | 10 |
| `-n` | Nombre del segmento de memoria compartida | `evaluator` |
| `-b` | Reactivo inicial y máximo de sangre (Blood) | 100 |
| `-d` | Reactivo inicial y máximo de detritos (Detritus) | 100 |
| `-s` | Reactivo inicial y máximo de piel (Skin) | 100 |
| `-q` | Tamaño de cada cola interna (una por tipo de muestra) | 6 |
| `-ee` | Tiempo máximo de espera de una muestra, en segundos (ver nota) | 60 |

Todos los valores enteros deben ser mayores que cero. Si no lo son, `init` imprime el error y termina con código 1.

> **Supuestos que proponemos al docente** (ver [Proyecto, pruebas y reparto](Proyecto-pruebas-y-trabajo)):
> 1. `-ee` aparece en la sinopsis pero el enunciado no lo describe. Lo definimos como el **tiempo máximo que una muestra puede esperar** antes de ser descartada con resultado `?`.
> 2. `-s` significa «reactivo de piel» en `init` y «nombre del segmento» en `ctrl` y `rep`. Se respeta tal cual la sinopsis del enunciado. `reg` usa `-n`.

### `reg`, `ctrl` y `rep`

| Comando | Opciones |
|---|---|
| `reg` | `-n` nombre del segmento. Después, uno o más archivos, o `-` para leer de la entrada estándar. |
| `ctrl` | `-s` nombre del segmento. |
| `rep` | `-s` nombre del segmento. `-i <segundos>` consume durante ese tiempo. `-m <cantidad>` consume esa cantidad de reportes. Son excluyentes y una de las dos es obligatoria. |

## 3. Variables de ambiente

Las opciones de la línea de comandos tienen prioridad sobre las variables.

| Variable | Uso | Por omisión |
|---|---|---|
| `EVAL_SHM_NAME` | Nombre del segmento si no se pasa `-n` o `-s` | `evaluator` |
| `EVAL_ROBOT_CAPACITY` | Unidades de reactivo que el robot lleva por viaje | 50 |
| `EVAL_MAX_WAIT` | Segundos máximos de espera (equivale a `-ee`) | 60 |
| `EVAL_TIME_SCALE` | Factor que divide todos los tiempos simulados, útil para pruebas rápidas | 1 |
| `EVAL_LOG` | `0` silencioso, `1` imprime los eventos del evaluador | 1 |

## 4. Códigos de salida

| Código | Significado |
|---|---|
| 0 | Éxito |
| 1 | Error de uso o de argumentos |
| 2 | El segmento no existe (no se ha ejecutado `init`) |
| 3 | Error del sistema operativo, o segmento inválido o incompatible |
| 4 | El sistema ya existe (solo en `init`) |
| 5 | El sistema terminó mientras el programa esperaba |
