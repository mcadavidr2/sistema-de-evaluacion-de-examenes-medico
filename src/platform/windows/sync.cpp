// Semáforos y mutex con nombre entre procesos en Windows.
//
// Son objetos del kernel, así que NO viven dentro del segmento. Dentro del segmento
// solo está su nombre. Cada proceso abre el objeto la primera vez que lo usa y guarda
// el handle en una tabla propia (los handles no se pueden compartir entre procesos).
#include <cstring>
#include <mutex>
#include <unordered_map>

#include "../platform.h"
#include "winutil.h"

namespace plat {

namespace {

std::mutex g_tableMutex;
std::unordered_map<std::string, HANDLE> g_sems;
std::unordered_map<std::string, HANDLE> g_mutexes;

void storeName(char (&dst)[96], const std::string& n) {
    std::strncpy(dst, n.c_str(), sizeof(dst) - 1);
    dst[sizeof(dst) - 1] = '\0';
}

HANDLE semHandle(PlatformSem& s) {
    std::lock_guard<std::mutex> lk(g_tableMutex);
    auto it = g_sems.find(s.name);
    if (it != g_sems.end()) return it->second;
    HANDLE h = OpenSemaphoreW(SEMAPHORE_ALL_ACCESS, FALSE, objectName(s.name).c_str());
    if (h) g_sems[s.name] = h;
    return h;
}

HANDLE mutexHandle(PlatformMutex& m) {
    std::lock_guard<std::mutex> lk(g_tableMutex);
    auto it = g_mutexes.find(m.name);
    if (it != g_mutexes.end()) return it->second;
    HANDLE h = OpenMutexW(MUTEX_ALL_ACCESS, FALSE, objectName(m.name).c_str());
    if (h) g_mutexes[m.name] = h;
    return h;
}

void forget(std::unordered_map<std::string, HANDLE>& table, const char* name) {
    std::lock_guard<std::mutex> lk(g_tableMutex);
    auto it = table.find(name);
    if (it == table.end()) return;
    CloseHandle(it->second);
    table.erase(it);
}

}  // namespace

// ---- Semáforos -------------------------------------------------------------
void semInit(PlatformSem& s, const std::string& objName, unsigned initial, unsigned maxCount) {
    storeName(s.name, objName);
    HANDLE h = CreateSemaphoreW(nullptr, static_cast<LONG>(initial), static_cast<LONG>(maxCount),
                                objectName(s.name).c_str());
    if (h) {
        std::lock_guard<std::mutex> lk(g_tableMutex);
        g_sems[s.name] = h;
    }
}

Wait semWait(PlatformSem& s, int timeoutMs) {
    HANDLE h = semHandle(s);
    if (!h) return Wait::Error;
    const DWORD ms = timeoutMs < 0 ? INFINITE : static_cast<DWORD>(timeoutMs);

    // Se espera el semáforo y el evento de terminación a la vez: los procesos se enteran
    // de que el sistema termina al instante, sin esperar el plazo.
    HANDLE ev = g_shutdownEvent;
    HANDLE hs[2] = {h, ev};
    const DWORD n = ev ? 2 : 1;
    DWORD r = WaitForMultipleObjects(n, hs, FALSE, ms);
    if (r == WAIT_OBJECT_0) return Wait::Ok;  // si ambos están señalados gana el semáforo
    if (r == WAIT_OBJECT_0 + 1 && n == 2) return Wait::Shutdown;
    if (r == WAIT_TIMEOUT) return Wait::Timeout;
    return Wait::Error;
}

bool semPost(PlatformSem& s) {
    HANDLE h = semHandle(s);
    return h && ReleaseSemaphore(h, 1, nullptr) != 0;
}

void semDestroy(PlatformSem& s) { forget(g_sems, s.name); }

// ---- Mutex entre procesos ---------------------------------------------------
// Un mutex de Windows lo debe liberar el mismo hilo que lo tomó. El protocolo lo cumple:
// cada mutex se toma y se suelta dentro de la misma función (clase Guard).
void mutexInit(PlatformMutex& m, const std::string& objName) {
    storeName(m.name, objName);
    HANDLE h = CreateMutexW(nullptr, FALSE, objectName(m.name).c_str());
    if (h) {
        std::lock_guard<std::mutex> lk(g_tableMutex);
        g_mutexes[m.name] = h;
    }
}

Lock mutexLock(PlatformMutex& m) {
    HANDLE h = mutexHandle(m);
    if (!h) return Lock::Error;
    DWORD r = WaitForSingleObject(h, INFINITE);
    if (r == WAIT_OBJECT_0) return Lock::Ok;
    if (r == WAIT_ABANDONED) return Lock::Recovered;  // el dueño murió; el mutex ahora es nuestro
    return Lock::Error;
}

void mutexUnlock(PlatformMutex& m) {
    HANDLE h = mutexHandle(m);
    if (h) ReleaseMutex(h);
}

void mutexDestroy(PlatformMutex& m) { forget(g_mutexes, m.name); }

void notifyShutdown() {
    if (g_shutdownEvent) SetEvent(g_shutdownEvent);
}

}  // namespace plat
