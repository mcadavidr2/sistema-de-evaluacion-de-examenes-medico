# Instalación

El proyecto compila y se ejecuta en **Linux** y en **Windows**. Los binarios no se suben al repositorio: se generan con `make`.

## Linux

Requisitos: `g++` 9 o superior (C++17) y `make`.

```
sudo apt install g++ make        # Debian / Ubuntu
make                             # genera bin/
make test                        # plan de pruebas
```

### Instalar en un directorio distinto al local

```
make install PREFIX=$HOME/evaluador-inst        # copia los 5 binarios a $PREFIX/bin
export PATH=$PREFIX/bin:$PATH
evaluator init
```

Los cinco binarios deben quedar juntos: `evaluator` busca `eval-init`, `eval-reg`, `eval-ctrl` y `eval-rep` en su mismo directorio.

Comprobaciones útiles: `ls -l /dev/shm/` (segmentos), `ps -L -p <pid>` (hilos de `init`). Un segmento huérfano se borra con `rm /dev/shm/evaluator`.

## Windows

Requisitos: **MSYS2** (https://www.msys2.org) con el compilador MinGW-w64 de la variante *posix threads* (la variante *win32* no trae `std::thread`).

1. Instale MSYS2 y abra la consola **MSYS2 UCRT64** (no la MSYS ni la MINGW32).
2. Instale las herramientas:
   ```
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-make make
   ```
3. Compile desde la raíz del proyecto:
   ```
   mingw32-make
   ```
   Genera `bin\evaluator.exe`, `bin\eval-init.exe`, `bin\eval-reg.exe`, `bin\eval-ctrl.exe` y `bin\eval-rep.exe`, enlazados de forma estática (no necesitan DLL de MinGW).
4. Pruebas: `bash tests/run_tests.sh`. Las pruebas 8 y 10 (señales) se hacen a mano en Windows: `Ctrl+C` en la consola de `init` (los demás programas deben salir con código 5) y terminar `eval-init.exe` desde el Administrador de tareas.
5. Para instalar en otro directorio: `mingw32-make install PREFIX=C:/evaluador-inst` y agregue `C:\evaluador-inst\bin` al `PATH`.

Notas para Windows:

- En `ctrl`, el fin de entrada es `Ctrl+Z` y `Enter` (en Linux es `Ctrl+D`).
- Los archivos de muestras pueden terminar sus líneas en `CRLF`; `reg` lo acepta.
- Comprobaciones: `tasklist | findstr eval` (procesos) y Process Explorer buscando `Local\<nombre>_...` (objetos con nombre).
- Si el programa se cierra al arrancar con un error de enlace estático y hilos, enlace `winpthread` completo: agregue `-Wl,-Bstatic,--whole-archive -lwinpthread -Wl,--no-whole-archive` a `LDFLAGS_OS` en el `Makefile`.

## Desarrollar la parte de Windows en un Mac con chip Apple (M1 a M5)

Los `.exe` no se ejecutan en macOS, así que se necesita un Windows:

1. **Máquina virtual con Windows 11 ARM** (Parallels Desktop, VMware Fusion o UTM). Windows 11 ARM ejecuta programas x64 por emulación, así que MSYS2 UCRT64 (x64) funciona normalmente. Siga los pasos de la sección Windows dentro de la máquina virtual.
2. Compilación cruzada desde macOS (opcional): `brew install mingw-w64` y luego `make TARGET_OS=windows CXX=x86_64-w64-mingw32-g++`. Genera los `.exe`, que igualmente hay que ejecutar en Windows.

macOS tampoco es Linux: el código de Linux usa semáforos POSIX sin nombre (`sem_init`) y `sem_timedwait`, que macOS no ofrece. Para probar la parte de Linux en un Mac hace falta una máquina virtual o Docker con Linux.
