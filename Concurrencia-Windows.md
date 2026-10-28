# Concurrencia en Windows

**Responsable:** Sebastian Vasquez

[Inicio](Home) · Anterior: [Concurrencia en Linux](Concurrencia-Linux) · Siguiente: [Mecanismo de los comandos](Mecanismo-de-los-comandos)

Esta página aplica a Windows el diseño de [Concurrencia común](Concurrencia-comun). Todo el código está en `src/platform/windows/`.

## 1. Primitivas usadas

| Concepto | Primitiva en Windows |
|---|---|
| Segmento compartido | `CreateFileMappingW` + `MapViewOfFile` (`OpenFileMappingW` en los clientes) |
| Semáforos entre procesos | `CreateSemaphoreW` con nombre (`OpenSemaphoreW` en los clientes) |
| Esperar con límite | `WaitForSingleObject(handle, ms)` o `WaitForMultipleObjects` |
| Avisar | `ReleaseSemaphore(handle, 1, NULL)` |
| Mutex entre procesos | `CreateMutexW` con nombre (`OpenMutexW` en los clientes) |
| Señal de terminación | `SetConsoleCtrlHandler` |
| Hilos | `std::thread`, `std::mutex`, `std::condition_variable` |

## 2. Segmento

- **Nombre:** `Local\<nombre>`, por ejemplo `Local\evaluator`.
- **Creación en `init`:** `CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, alto, bajo, nombre)` y `MapViewOfFile`.
- **Apertura en `reg`, `ctrl` y `rep`:** `OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, nombre)`. Si falla porque no existe, el sistema no existe y el programa termina con código 2.
- **Sistema que ya existe:** si `CreateFileMappingW` devuelve un handle y `GetLastError() == ERROR_ALREADY_EXISTS`, `init` cierra el handle, imprime «el sistema ya existe» y termina con código 4.
- **No hay segmentos huérfanos.** El kernel destruye el mapeo cuando se cierra el último handle, aunque el proceso muera. Por eso Windows no necesita la comprobación de `evaluatorPid` que sí usa Linux. El campo se conserva solo como diagnóstico.

## 3. Semáforos y mutex con nombre

Los objetos son del kernel, así que **no** viven dentro del segmento. Cada proceso los abre por nombre. Los nombres se derivan del nombre del segmento (`<nombre>`):

| Objeto lógico | Nombre del objeto en Windows |
|---|---|
| `inEmpty[k]` | `Local\<nombre>_inEmpty_<k>` |
| `inFull[k]` | `Local\<nombre>_inFull_<k>` |
| `inMutex[k]` | `Local\<nombre>_inMutex_<k>` |
| `outEmpty` | `Local\<nombre>_outEmpty` |
| `outFull` | `Local\<nombre>_outFull` |
| `outMutex` | `Local\<nombre>_outMutex` |
| `idMutex` | `Local\<nombre>_idMutex` |
| `reactiveMutex` | `Local\<nombre>_reactiveMutex` |
| `statusMutex` | `Local\<nombre>_statusMutex` |
| aviso de terminación | `Local\<nombre>_shutdown` (evento manual) |

Detalles:

- **Semáforos:** el valor inicial es el de [Concurrencia común](Concurrencia-comun). El valor máximo es la capacidad de la cola (`ie` o `oe`).
- **Orden de creación:** `init` crea todos los objetos antes de escribir `Header.magic`, como en Linux.
- **Mutex abandonado:** si un proceso muere con un mutex tomado, el siguiente que lo espera recibe `WAIT_ABANDONED`. Ese caso se trata como «tomado», se valida la cola afectada y se continúa.
- **Propiedad:** un mutex de Windows debe liberarlo el mismo hilo que lo tomó. El protocolo cumple esto, porque cada mutex se toma y se suelta dentro de la misma función.
- Los mutex solo se usan entre procesos. Entre hilos de `init` se usan `std::mutex` y `std::condition_variable`.

### Esperas con límite

```
resultado = WaitForMultipleObjects(2, {semáforo, evento_shutdown}, FALSE, 500)
WAIT_OBJECT_0     → se obtuvo el semáforo
WAIT_OBJECT_0 + 1 → el sistema está terminando
WAIT_TIMEOUT      → volver a revisar shuttingDown y repetir
```

Con el evento de terminación, los procesos cliente se enteran al instante sin esperar los 500 ms.

## 4. Terminación

- `init` registra `SetConsoleCtrlHandler(manejador, TRUE)` para `CTRL_C_EVENT`, `CTRL_BREAK_EVENT` y `CTRL_CLOSE_EVENT`.
- El manejador corre en un **hilo propio** que crea Windows. Debe ser breve: pone `Header.shuttingDown = 1`, llama a `SetEvent(shutdown)` y devuelve `TRUE`.
- El hilo principal ejecuta la [terminación ordenada](Concurrencia-comun) y luego libera recursos.

### Limpieza al terminar `init`

```
join de todos los hilos
CloseHandle(...) de cada semáforo, mutex y evento
UnmapViewOfFile(segmento)
CloseHandle(mapeo)
```

Si todavía hay clientes con el mapeo abierto, el segmento sigue existiendo hasta que ellos lo cierren. Los clientes lo hacen al detectar `shuttingDown`.

## 5. Diferencias de formato de texto

- Los archivos de muestras pueden terminar cada línea en `CRLF`. `reg` elimina el `\r` antes de interpretar la línea.
- Los archivos `.spl` que escribe `reg` terminan las líneas con `\n`.
- En `ctrl`, el fin de entrada se da con `Ctrl+Z` y `Enter`. En Linux es `Ctrl+D`.

## 6. Wrapper

`evaluator.exe` usa `CreateProcessW` para lanzar `bin\eval-<comando>.exe` con los mismos argumentos y los mismos handles de entrada y salida estándar. Espera con `WaitForSingleObject` y termina con el código que devuelve `GetExitCodeProcess`.

## 7. Compilación

```
g++ -std=c++17 -O2 -Wall -Wextra -static -static-libgcc -static-libstdc++ ...
```

- Se usa MinGW-w64 y `mingw32-make`.
- Enlazar de forma estática evita depender de DLL de MinGW en el equipo donde se ejecute.
- Se compila con `UNICODE` y `_UNICODE` definidos, y se usan las funciones `W` de la API.
- El `Makefile` genera `bin\evaluator.exe`, `bin\eval-init.exe`, `bin\eval-reg.exe`, `bin\eval-ctrl.exe` y `bin\eval-rep.exe`.

## 8. Comprobaciones útiles

| Qué | Cómo |
|---|---|
| Ver los hilos de `init` | Administrador de tareas → Detalles → agregar la columna de hilos |
| Ver los handles abiertos | Process Explorer, buscando los nombres `Local\<nombre>_...` |
| Comprobar que no quedan procesos | `tasklist \| findstr eval` |
