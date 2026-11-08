// Utilidades internas de la capa Windows (no forman parte de la interfaz platform.h).
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>

namespace plat {

// Convierte UTF-8 a UTF-16, que es lo que esperan las funciones "W" de la API.
inline std::wstring widen(const std::string& s) {
    if (s.empty()) return std::wstring();
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

// Todos los objetos del sistema viven en el espacio de nombres de la sesión: "Local\<nombre>".
inline std::wstring objectName(const std::string& n) { return L"Local\\" + widen(n); }

// Evento manual "<segmento>_shutdown". Lo crea init y los clientes lo abren.
// Se define en shm.cpp.
extern HANDLE g_shutdownEvent;

}  // namespace plat
