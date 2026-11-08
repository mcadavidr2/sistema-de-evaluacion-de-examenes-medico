// Memoria compartida del evaluador: estructuras, disposición del segmento y
// operaciones sobre las colas de entrada y de salida.
// Este código es idéntico en Linux y Windows; solo usa la interfaz de platform/.
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

#include "../platform/platform.h"

namespace ev {

constexpr uint32_t kMagic      = 0x45564C31u;  // "EVL1"
constexpr uint32_t kVersion    = 1;
constexpr int      kMaxNameLen = 40;           // los nombres de objetos de Windows son cortos

// Códigos de salida de todos los programas.
enum ExitCode : int {
    kOk         = 0,
    kUsage      = 1,  // error de uso o de argumentos
    kNoSystem   = 2,  // el segmento no existe (no se ha ejecutado init)
    kOsError    = 3,  // error del sistema operativo, o segmento inválido
    kExists     = 4,  // el sistema ya existe (solo init)
    kTerminated = 5   // el sistema terminó mientras el programa esperaba
};

enum State : uint8_t { FREE = 0, HELD, IN_INTERNAL, PROCESSING };

// Una posición de una cola (entrada o salida).
struct Slot {
    uint64_t id;
    uint8_t  inQueue;      // cola de entrada de origen (0..i-1)
    char     type;         // 'B' | 'D' | 'S'
    uint8_t  qty;          // cantidad de muestra (1..5)
    char     result;       // 'P' | 'N' | '?'  (solo en la cola de salida)
    int64_t  enqueueTime;  // milisegundos desde epoch al registrar
};

struct Header {
    uint32_t magic, version;                        // magic se escribe al final de init
    uint32_t numIn, inSize, outSize, internalSize;  // i, ie, oe, q
    uint32_t maxWait;                               // -ee, en segundos
    int32_t  evaluatorPid;                          // pid de init
    uint8_t  shuttingDown;                          // 1 = el sistema está terminando
    uint64_t nextId;                                // contador de identificadores
    int32_t  reactive[3];                           // niveles actuales B, D, S
    int32_t  reactiveMax[3];                        // máximos (-b, -d, -s)
    PlatformMutex idMutex;
    PlatformMutex reactiveMutex;
    PlatformMutex statusMutex;
};

struct InQueueCtl {
    uint32_t      head, tail, count;
    PlatformSem   empty, full;
    PlatformMutex mutex;
};

struct OutQueueCtl {
    uint32_t      head, tail, count;
    PlatformSem   empty, full;
    PlatformMutex mutex;
};

// Lo que ctrl necesita ver de la memoria privada de init.
struct StatusEntry {
    State    state;
    uint8_t  inQueue;
    char     type;
    uint8_t  qty;
    uint8_t  procTime;  // tiempo de procesamiento asignado, en segundos
    uint64_t id;
    int64_t  enqueueTime;
};

// Parámetros con los que init crea el sistema.
struct Params {
    std::string name = "evaluator";
    uint32_t    numIn = 5, inSize = 6, outSize = 10, internalSize = 6;
    uint32_t    maxWait = 60;
    int32_t     reactiveMax[3] = {100, 100, 100};
};

inline int typeIndex(char t) { return t == 'B' ? 0 : t == 'D' ? 1 : t == 'S' ? 2 : -1; }
inline char typeChar(int i) { return "BDS"[i]; }

enum class QStatus { Ok, Timeout, Shutdown, Error };

// Toma un mutex entre procesos mientras viva el objeto. `recovered()` indica que el
// dueño anterior murió con él tomado.
class Guard {
public:
    explicit Guard(PlatformMutex& m) : m_(m), r_(plat::mutexLock(m)) {
        if (r_ == plat::Lock::Error) throw std::runtime_error("no se pudo tomar un mutex");
    }
    ~Guard() { plat::mutexUnlock(m_); }
    Guard(const Guard&) = delete;
    Guard& operator=(const Guard&) = delete;
    bool recovered() const { return r_ == plat::Lock::Recovered; }

private:
    PlatformMutex& m_;
    plat::Lock     r_;
};

struct Layout {
    size_t inOff, inStride, outOff, outStride, statusOff, statusCount, total;
};
Layout computeLayout(uint32_t numIn, uint32_t inSize, uint32_t outSize, uint32_t q);

class Shared {
public:
    // init: crea e inicializa todo. Devuelve un ExitCode (kOk si salió bien).
    int create(const Params& p);
    // reg, ctrl, rep: abre un sistema existente. Devuelve un ExitCode.
    int attach(const std::string& name);
    void close();    // suelta el segmento en este proceso
    void destroy();  // init: destruye semáforos y mutex, y elimina el segmento

    Header* hdr() const { return hdr_; }
    const std::string& name() const { return name_; }

    bool shuttingDown() const;
    void setShuttingDown();

    // Colas. timeoutMs < 0 bloquea hasta que haya espacio/elemento o termine el sistema.
    QStatus pushIn(uint32_t k, const Slot& s, int timeoutMs = -1);
    QStatus popIn(uint32_t k, Slot& out, int timeoutMs);
    QStatus pushOut(const Slot& s, int timeoutMs = -1);
    QStatus popOut(Slot& out, int timeoutMs);

    uint64_t nextId();

    // Acceso directo (ctrl recorre las colas bajo su mutex).
    InQueueCtl*  inCtl(uint32_t k) const;
    Slot*        inSlots(uint32_t k) const;
    OutQueueCtl* outCtl() const;
    Slot*        outSlots() const;
    StatusEntry* statusAt(size_t i) const;
    size_t       statusCount() const { return L_.statusCount; }
    PlatformMutex& reactiveMutex() const { return hdr_->reactiveMutex; }
    PlatformMutex& statusMutex() const { return hdr_->statusMutex; }

private:
    QStatus acquire(PlatformSem& s, int timeoutMs);
    std::string objName(const char* what, int k = -1) const;
    char* base() const { return static_cast<char*>(seg_.data()); }

    plat::ShmSegment seg_;
    Header*          hdr_ = nullptr;
    Layout           L_{};
    std::string      name_;
};

}  // namespace ev
