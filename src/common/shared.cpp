#include "shared.h"

#include <algorithm>
#include <chrono>

namespace ev {

namespace {
size_t alignUp(size_t v, size_t a = 64) { return (v + a - 1) / a * a; }
// Las posiciones (Slot) empiezan justo después de la estructura de control, alineadas.
constexpr size_t kCtlPad = 16;
}  // namespace

Layout computeLayout(uint32_t numIn, uint32_t inSize, uint32_t outSize, uint32_t q) {
    Layout L{};
    size_t off = alignUp(sizeof(Header));
    L.inOff    = off;
    L.inStride = alignUp(alignUp(sizeof(InQueueCtl), kCtlPad) + size_t(inSize) * sizeof(Slot));
    off += L.inStride * numIn;
    L.outOff    = off;
    L.outStride = alignUp(alignUp(sizeof(OutQueueCtl), kCtlPad) + size_t(outSize) * sizeof(Slot));
    off += L.outStride;
    L.statusOff   = off;
    L.statusCount = size_t(numIn) + 3 * size_t(q) + 3;
    off += alignUp(L.statusCount * sizeof(StatusEntry));
    L.total = off;
    return L;
}

std::string Shared::objName(const char* what, int k) const {
    std::string n = name_ + "_" + what;
    if (k >= 0) n += "_" + std::to_string(k);
    return n;
}

// ---------------------------------------------------------------------------
// Ciclo de vida
// ---------------------------------------------------------------------------
int Shared::create(const Params& p) {
    name_ = p.name;
    L_    = computeLayout(p.numIn, p.inSize, p.outSize, p.internalSize);

    auto alive = [](const void* d, size_t size) -> bool {
        if (size < sizeof(Header)) return true;  // otro init está arrancando
        const Header* h = static_cast<const Header*>(d);
        if (h->evaluatorPid <= 0) return true;
        return plat::pidAlive(h->evaluatorPid);
    };
    int rc = seg_.create(name_, L_.total, alive);
    if (rc != 0) return rc == 4 ? kExists : kOsError;

    hdr_               = static_cast<Header*>(seg_.data());
    hdr_->version      = kVersion;
    hdr_->numIn        = p.numIn;
    hdr_->inSize       = p.inSize;
    hdr_->outSize      = p.outSize;
    hdr_->internalSize = p.internalSize;
    hdr_->maxWait      = p.maxWait;
    hdr_->evaluatorPid = plat::currentPid();
    hdr_->shuttingDown = 0;
    hdr_->nextId       = 1;
    for (int t = 0; t < 3; ++t) {
        hdr_->reactiveMax[t] = p.reactiveMax[t];
        hdr_->reactive[t]    = p.reactiveMax[t];
    }
    plat::mutexInit(hdr_->idMutex, objName("idMutex"));
    plat::mutexInit(hdr_->reactiveMutex, objName("reactiveMutex"));
    plat::mutexInit(hdr_->statusMutex, objName("statusMutex"));

    for (uint32_t k = 0; k < p.numIn; ++k) {
        InQueueCtl* c = inCtl(k);
        c->head = c->tail = c->count = 0;
        plat::semInit(c->empty, objName("inEmpty", int(k)), p.inSize, p.inSize);
        plat::semInit(c->full, objName("inFull", int(k)), 0, p.inSize);
        plat::mutexInit(c->mutex, objName("inMutex", int(k)));
    }
    OutQueueCtl* o = outCtl();
    o->head = o->tail = o->count = 0;
    plat::semInit(o->empty, objName("outEmpty"), p.outSize, p.outSize);
    plat::semInit(o->full, objName("outFull"), 0, p.outSize);
    plat::mutexInit(o->mutex, objName("outMutex"));

    // La tabla de estado ya está en cero (FREE). magic va al final: antes de esto
    // ningún cliente considera que el sistema esté listo.
    __atomic_store_n(&hdr_->magic, kMagic, __ATOMIC_RELEASE);
    return kOk;
}

int Shared::attach(const std::string& name) {
    name_  = name;
    int rc = seg_.open(name_);
    if (rc == 2) return kNoSystem;
    if (rc != 0) return kOsError;
    if (seg_.size() < sizeof(Header)) {
        seg_.close();
        return kOsError;
    }
    hdr_ = static_cast<Header*>(seg_.data());
    if (__atomic_load_n(&hdr_->magic, __ATOMIC_ACQUIRE) != kMagic || hdr_->version != kVersion) {
        seg_.close();
        hdr_ = nullptr;
        return kOsError;
    }
    if (!plat::pidAlive(hdr_->evaluatorPid)) {  // residuo de un init que murió
        seg_.close();
        hdr_ = nullptr;
        return kNoSystem;
    }
    L_ = computeLayout(hdr_->numIn, hdr_->inSize, hdr_->outSize, hdr_->internalSize);
    if (seg_.size() < L_.total) {
        seg_.close();
        hdr_ = nullptr;
        return kOsError;
    }
    if (shuttingDown()) {
        seg_.close();
        hdr_ = nullptr;
        return kTerminated;
    }
    return kOk;
}

void Shared::close() {
    seg_.close();
    hdr_ = nullptr;
}

void Shared::destroy() {
    if (hdr_) {
        for (uint32_t k = 0; k < hdr_->numIn; ++k) {
            InQueueCtl* c = inCtl(k);
            plat::semDestroy(c->empty);
            plat::semDestroy(c->full);
            plat::mutexDestroy(c->mutex);
        }
        OutQueueCtl* o = outCtl();
        plat::semDestroy(o->empty);
        plat::semDestroy(o->full);
        plat::mutexDestroy(o->mutex);
        plat::mutexDestroy(hdr_->idMutex);
        plat::mutexDestroy(hdr_->reactiveMutex);
        plat::mutexDestroy(hdr_->statusMutex);
    }
    seg_.destroy();
    hdr_ = nullptr;
}

// ---------------------------------------------------------------------------
// Acceso a las regiones
// ---------------------------------------------------------------------------
InQueueCtl* Shared::inCtl(uint32_t k) const {
    return reinterpret_cast<InQueueCtl*>(base() + L_.inOff + size_t(k) * L_.inStride);
}
Slot* Shared::inSlots(uint32_t k) const {
    return reinterpret_cast<Slot*>(reinterpret_cast<char*>(inCtl(k)) + alignUp(sizeof(InQueueCtl), kCtlPad));
}
OutQueueCtl* Shared::outCtl() const { return reinterpret_cast<OutQueueCtl*>(base() + L_.outOff); }
Slot* Shared::outSlots() const {
    return reinterpret_cast<Slot*>(reinterpret_cast<char*>(outCtl()) + alignUp(sizeof(OutQueueCtl), kCtlPad));
}
StatusEntry* Shared::statusAt(size_t i) const {
    return reinterpret_cast<StatusEntry*>(base() + L_.statusOff) + i;
}

bool Shared::shuttingDown() const { return __atomic_load_n(&hdr_->shuttingDown, __ATOMIC_ACQUIRE) != 0; }
void Shared::setShuttingDown() { __atomic_store_n(&hdr_->shuttingDown, uint8_t(1), __ATOMIC_RELEASE); }

uint64_t Shared::nextId() {
    Guard g(hdr_->idMutex);
    return hdr_->nextId++;
}

// ---------------------------------------------------------------------------
// Espera de semáforos en tramos de 500 ms para revisar shuttingDown.
// ---------------------------------------------------------------------------
QStatus Shared::acquire(PlatformSem& s, int timeoutMs) {
    using namespace std::chrono;
    const auto start = steady_clock::now();
    for (;;) {
        if (shuttingDown()) return QStatus::Shutdown;
        int slice = 500;
        if (timeoutMs >= 0) {
            long long elapsed = duration_cast<milliseconds>(steady_clock::now() - start).count();
            long long left    = timeoutMs - elapsed;
            if (left <= 0) return QStatus::Timeout;
            slice = static_cast<int>(std::min<long long>(500, left));
        }
        switch (plat::semWait(s, slice)) {
            case plat::Wait::Ok: return QStatus::Ok;
            case plat::Wait::Shutdown: return QStatus::Shutdown;
            case plat::Wait::Error: return QStatus::Error;
            case plat::Wait::Timeout: break;
        }
    }
}

// Si un proceso murió con el mutex tomado, los índices pueden estar a medio actualizar.
template <typename Ctl>
static void repair(Ctl* c, uint32_t capacity) {
    if (c->count > capacity) c->count = capacity;
    c->head %= capacity;
    c->tail %= capacity;
}

// ---------------------------------------------------------------------------
// Protocolo productor / consumidor (ver doc/Bandeja-de-entrada.md y Bandeja-de-salida.md)
// ---------------------------------------------------------------------------
QStatus Shared::pushIn(uint32_t k, const Slot& s, int timeoutMs) {
    InQueueCtl* c = inCtl(k);
    QStatus st    = acquire(c->empty, timeoutMs);  // 1. esperar espacio (semáforo primero)
    if (st != QStatus::Ok) return st;
    {
        Guard g(c->mutex);  // 2. mutex solo para tocar head/tail/count
        if (g.recovered()) repair(c, hdr_->inSize);
        inSlots(k)[c->tail] = s;
        c->tail             = (c->tail + 1) % hdr_->inSize;
        c->count++;
    }
    plat::semPost(c->full);  // 3. avisar que hay un elemento
    return QStatus::Ok;
}

QStatus Shared::popIn(uint32_t k, Slot& out, int timeoutMs) {
    InQueueCtl* c = inCtl(k);
    QStatus st    = acquire(c->full, timeoutMs);
    if (st != QStatus::Ok) return st;
    {
        Guard g(c->mutex);
        if (g.recovered()) repair(c, hdr_->inSize);
        out     = inSlots(k)[c->head];
        c->head = (c->head + 1) % hdr_->inSize;
        c->count--;
    }
    plat::semPost(c->empty);
    return QStatus::Ok;
}

QStatus Shared::pushOut(const Slot& s, int timeoutMs) {
    OutQueueCtl* c = outCtl();
    QStatus st     = acquire(c->empty, timeoutMs);
    if (st != QStatus::Ok) return st;
    {
        Guard g(c->mutex);
        if (g.recovered()) repair(c, hdr_->outSize);
        outSlots()[c->tail] = s;
        c->tail             = (c->tail + 1) % hdr_->outSize;
        c->count++;
    }
    plat::semPost(c->full);
    return QStatus::Ok;
}

QStatus Shared::popOut(Slot& out, int timeoutMs) {
    OutQueueCtl* c = outCtl();
    QStatus st     = acquire(c->full, timeoutMs);
    if (st != QStatus::Ok) return st;
    {
        Guard g(c->mutex);
        if (g.recovered()) repair(c, hdr_->outSize);
        out     = outSlots()[c->head];
        c->head = (c->head + 1) % hdr_->outSize;
        c->count--;
    }
    plat::semPost(c->empty);
    return QStatus::Ok;
}

}  // namespace ev
