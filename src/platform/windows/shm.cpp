// Segmento de memoria compartida en Windows: CreateFileMappingW + MapViewOfFile.
#include "../platform.h"
#include "winutil.h"

namespace plat {

HANDLE g_shutdownEvent = nullptr;

int ShmSegment::create(const std::string& name, size_t size,
                       const std::function<bool(const void*, size_t)>&) {
    const std::wstring wn = objectName(name);
    const unsigned long long sz = size;
    HANDLE h = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                  static_cast<DWORD>(sz >> 32), static_cast<DWORD>(sz & 0xFFFFFFFFull),
                                  wn.c_str());
    if (!h) return 3;
    // GetLastError se lee justo después de la llamada. Si el mapeo ya existía, Windows
    // devuelve un handle al existente y ERROR_ALREADY_EXISTS.
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(h);
        return 4;
    }
    void* p = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (!p) {
        CloseHandle(h);
        return 3;
    }
    // Evento manual para avisar la terminación a los procesos que esperan semáforos.
    HANDLE ev = CreateEventW(nullptr, TRUE, FALSE, objectName(name + "_shutdown").c_str());
    if (!ev) {
        UnmapViewOfFile(p);
        CloseHandle(h);
        return 3;
    }
    g_shutdownEvent = ev;
    h_     = reinterpret_cast<std::intptr_t>(h);
    ev_    = reinterpret_cast<std::intptr_t>(ev);
    data_  = p;  // las páginas vienen en cero
    size_  = size;
    name_  = name;
    owner_ = true;
    return 0;
}

int ShmSegment::open(const std::string& name) {
    HANDLE h = OpenFileMappingW(FILE_MAP_ALL_ACCESS, FALSE, objectName(name).c_str());
    if (!h) return GetLastError() == ERROR_FILE_NOT_FOUND ? 2 : 3;
    void* p = MapViewOfFile(h, FILE_MAP_ALL_ACCESS, 0, 0, 0);  // 0 = todo el mapeo
    if (!p) {
        CloseHandle(h);
        return 3;
    }
    MEMORY_BASIC_INFORMATION mbi{};
    if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0) {
        UnmapViewOfFile(p);
        CloseHandle(h);
        return 3;
    }
    // El evento puede no existir todavía si init está arrancando; entonces las esperas
    // se enteran de la terminación por el sondeo de 500 ms.
    HANDLE ev = OpenEventW(SYNCHRONIZE, FALSE, objectName(name + "_shutdown").c_str());
    g_shutdownEvent = ev;
    h_     = reinterpret_cast<std::intptr_t>(h);
    ev_    = reinterpret_cast<std::intptr_t>(ev);
    data_  = p;
    size_  = mbi.RegionSize;  // redondeado a página; basta para validar
    name_  = name;
    owner_ = false;
    return 0;
}

void ShmSegment::close() {
    if (data_) UnmapViewOfFile(data_);
    if (h_) CloseHandle(reinterpret_cast<HANDLE>(h_));
    if (ev_) {
        if (g_shutdownEvent == reinterpret_cast<HANDLE>(ev_)) g_shutdownEvent = nullptr;
        CloseHandle(reinterpret_cast<HANDLE>(ev_));
    }
    data_ = nullptr;
    size_ = 0;
    h_    = 0;
    ev_   = 0;
}

// Windows no necesita "unlink": el kernel destruye el mapeo cuando se cierra el último handle.
void ShmSegment::destroy() {
    owner_ = false;
    close();
}

}  // namespace plat
