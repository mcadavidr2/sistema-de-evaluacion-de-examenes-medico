# Evaluador de exámenes médicos

Simulación de un sistema paralelo de evaluación de exámenes médicos. ST0257 Sistemas Operativos, 2026-2.

Varios procesos (`init`, `reg`, `ctrl`, `rep`) se comunican por **memoria compartida, semáforos y mutex**. Dentro de `init` corren **hilos**: uno por cola de entrada, uno por tipo de muestra (Blood, Detritus, Skin) y uno para el robot que recarga los reactivos.

## Integrantes

| Integrante | Sistema a cargo |
|---|---|
| Mateo Cadavid | Linux |
| Sebastian Vasquez | Windows |

## Herramientas y bibliotecas

- **Lenguaje:** C++17. Solo biblioteca estándar y la API del sistema (POSIX en Linux, Win32 en Windows). No hay dependencias externas.
- **Linux:** `g++` 9 o superior, `make`.
- **Windows:** MSYS2 con `mingw-w64-ucrt-x86_64-gcc` y `mingw-w64-ucrt-x86_64-make` (MinGW-w64 con *posix threads*, necesario para `std::thread`).

Instrucciones completas en [Install.md](Install.md).

## Compilar y probar

```
make            # Linux
mingw32-make    # Windows (consola MSYS2 UCRT64)
make test       # plan de pruebas (en Windows: bash tests/run_tests.sh)
```

Los binarios quedan en `bin/` (no se suben al repositorio): `evaluator`, `eval-init`, `eval-reg`, `eval-ctrl`, `eval-rep`. Solo se invoca `evaluator`.

## Uso

Guía completa, paso a paso y con lo que se debe ver en cada paso: [doc/Guia-de-ejecucion.md](doc/Guia-de-ejecucion.md).

```
evaluator init [-i <n>] [-ie <n>] [-oe <n>] [-n <nombre>] [-b <n>] [-d <n>] [-s <n>] [-q <n>] [-ee <seg>]
evaluator reg  [-n <nombre>] { <archivo> ... | - }
evaluator ctrl [-s <nombre>]
evaluator rep  [-s <nombre>] { -i <segundos> | -m <cantidad> }
```

Ejemplo (cada comando en su propia consola; con `EVAL_TIME_SCALE=10` todo va 10 veces más rápido):

```
evaluator init                                  # consola 1: ocupa la consola
evaluator reg examples/muestras1.txt            # consola 2: crea examples/muestras1.spl con los ids
echo "0 B 2" | evaluator reg -                  #            o interactivo: imprime el id
evaluator ctrl                                  # consola 3: > list all   > update B 20
evaluator rep -m 5                              # consola 4: espera y muestra 5 reportes
```

Formato de un registro: `<cola> {B|D|S} <cantidad>` (cola `0..i-1`, cantidad `1..5`).

### Variables de ambiente

| Variable | Uso | Por omisión |
|---|---|---|
| `EVAL_SHM_NAME` | Nombre del segmento si no se pasa `-n` o `-s` | `evaluator` |
| `EVAL_ROBOT_CAPACITY` | Unidades de reactivo por viaje del robot | 50 |
| `EVAL_MAX_WAIT` | Espera máxima de una muestra, en segundos (equivale a `-ee`) | 60 |
| `EVAL_TIME_SCALE` | Divide los tiempos simulados (útil para pruebas) | 1 |
| `EVAL_LOG` | `0` silencioso, `1` imprime eventos de `init` | 1 |

### Códigos de salida

| Código | Significado |
|---|---|
| 0 | Éxito |
| 1 | Error de uso o de argumentos |
| 2 | El segmento no existe (no se ha ejecutado `init`) |
| 3 | Error del sistema operativo, o segmento inválido |
| 4 | El sistema ya existe (solo `init`) |
| 5 | El sistema terminó mientras el programa esperaba |

## Estructura

```
evaluador/
├── Makefile
├── Readme.md
├── Install.md
├── src/        código fuente (common/, init/, reg/, ctrl/, rep/, platform/linux, platform/windows)
├── bin/        binarios (no se suben)
├── doc/        documentación de diseño en Markdown (empezar por Home.md e Implementacion.md)
├── examples/   archivos de muestras y scripts de demostración
└── tests/      plan de pruebas automatizado
```
