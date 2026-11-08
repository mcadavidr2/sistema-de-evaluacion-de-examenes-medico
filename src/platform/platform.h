// Interfaz de la capa platform/. Es lo único que depende del sistema operativo.
// El código de common/, init/, reg/, ctrl/ y rep/ solo usa estas funciones.
//
//   Linux   -> src/platform/linux/    (Mateo)
//   Windows -> src/platform/windows/  (Sebastian)
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#if defined(_WIN32)
#include "windows/types.h"
#else
#include "linux/types.h"
#endif

namespace plat {

// Resultado de esperar un semáforo.
enum class Wait { Ok, Timeout, Shutdown, Error };
// Resultado de tomar un mutex entre procesos.
//   Recovered: el dueño anterior murió con el mutex tomado; ahora es nuestro
//              y hay que revisar la estructura que protegía.
enum class Lock { Ok, Recovered, Error };

// ---------------------------------------------------------------------------
// Segmento de memoria compartida
// ---------------------------------------------------------------------------
class ShmSegment {
public:
    ShmSegment() = default;
    ShmSegment(const ShmSegment&) = delete;
    ShmSegment& operator=(const ShmSegment&) = delete;
    ~ShmSegment() { close(); }

    // Crea un segmento nuevo de `size` bytes, en cero.
    //   0 = creado, 4 = ya existe, 3 = error del sistema.
    // Linux: si el segmento existe, `ownerAlive(datos, tamaño)` dice si su dueño sigue
    // vivo; si no, se elimina el residuo y se crea de nuevo. Windows lo ignora
    // (el kernel destruye el segmento cuando se cierra el último handle).
    int create(const std::string& name, size_t size,
               const std::function<bool(const void*, size_t)>& ownerAlive);

    // Abre un segmento existente. 0 = ok, 2 = no existe, 3 = error.
    int open(const std::string& name);

    void*  data() const { return data_; }
    size_t size() const { return size_; }

    void close();    // suelta el segmento en este proceso
    void destroy();  // el creador lo elimina (Linux: shm_unlink) y lo suelta

private:
    void*         data_  = nullptr;
    size_t        size_  = 0;
    std::string   name_;
    bool          owner_ = false;
    std::intptr_t h_     = 0;  // Windows: handle del mapeo
    std::intptr_t ev_    = 0;  // Windows: evento de terminación
};

// ---------------------------------------------------------------------------
// Semáforos y mutex entre procesos. Los tipos PlatformSem y PlatformMutex viven
// DENTRO del segmento compartido. `objName` es el nombre lógico del objeto
// (<segmento>_inEmpty_3, ...). Linux lo ignora. Windows lo usa para el objeto
// del kernel y lo guarda dentro de la estructura para que cualquier proceso
// pueda abrirlo.
// ---------------------------------------------------------------------------
void semInit(PlatformSem& s, const std::string& objName, unsigned initial, unsigned maxCount);
// timeoutMs < 0 espera sin límite.
Wait semWait(PlatformSem& s, int timeoutMs);
bool semPost(PlatformSem& s);
void semDestroy(PlatformSem& s);

void mutexInit(PlatformMutex& m, const std::string& objName);
Lock mutexLock(PlatformMutex& m);
void mutexUnlock(PlatformMutex& m);
void mutexDestroy(PlatformMutex& m);

// Despierta a los procesos que esperan semáforos porque el sistema termina.
// Windows: SetEvent. Linux: no hace nada (las esperas son de 500 ms como máximo).
void notifyShutdown();

// ---------------------------------------------------------------------------
// Procesos y señales
// ---------------------------------------------------------------------------
// Llamar al inicio de main. Windows: pone la consola en UTF-8 (para que los acentos de los
// mensajes se vean bien) y la restaura al salir. Linux: no hace nada.
void initConsole();

int  currentPid();
bool pidAlive(int pid);

// Solo para init. Debe llamarse en el hilo principal ANTES de crear otros hilos.
// Linux: bloquea SIGINT/SIGTERM. Windows: instala SetConsoleCtrlHandler.
void termInstall();
// Espera hasta `ms` milisegundos. Devuelve true si se pidió terminar.
bool termWait(int ms);
// Linux: ignora SIGPIPE (rep redirigido a un proceso que termina). Windows: nada.
void ignoreBrokenPipe();

// Directorio del ejecutable actual, terminado en separador.
std::string selfDir();
// Wrapper: lanza `path` con `args` y devuelve su código de salida.
// Linux: execv (solo vuelve si falla, con -1). Windows: CreateProcess y espera.
int runProgram(const std::string& path, const std::vector<std::string>& args);

}  // namespace plat
