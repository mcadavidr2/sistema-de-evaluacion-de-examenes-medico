// Procesos, terminación y lanzamiento de programas en Windows.
#include <cstdlib>

#include "../platform.h"
#include "winutil.h"

namespace plat {

namespace {

HANDLE g_termEvent = nullptr;

BOOL WINAPI termHandler(DWORD type) {
    // Windows ejecuta el manejador en un hilo propio. Debe ser breve: solo avisa al hilo principal.
    if (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT || type == CTRL_CLOSE_EVENT) {
        if (g_termEvent) SetEvent(g_termEvent);
        // Al cerrar la ventana, Windows mata el proceso unos 5 s después de que el manejador
        // devuelva; se espera un poco para dar tiempo a la terminación ordenada.
        if (type == CTRL_CLOSE_EVENT) Sleep(4000);
        return TRUE;
    }
    return FALSE;
}

// El wrapper no debe morir con Ctrl+C: el programa real recibe la misma señal y es
// quien termina, y el wrapper debe devolver su código de salida.
BOOL WINAPI swallowHandler(DWORD type) {
    return (type == CTRL_C_EVENT || type == CTRL_BREAK_EVENT) ? TRUE : FALSE;
}

// Reglas de CommandLineToArgvW para citar un argumento.
std::wstring quoteArg(const std::wstring& a) {
    if (!a.empty() && a.find_first_of(L" \t\n\v\"") == std::wstring::npos) return a;
    std::wstring r = L"\"";
    for (size_t i = 0;; ++i) {
        size_t backslashes = 0;
        while (i < a.size() && a[i] == L'\\') {
            ++i;
            ++backslashes;
        }
        if (i == a.size()) {
            r.append(backslashes * 2, L'\\');  // las barras finales se duplican antes de la comilla de cierre
            break;
        } else if (a[i] == L'"') {
            r.append(backslashes * 2 + 1, L'\\');
            r.push_back(L'"');
        } else {
            r.append(backslashes, L'\\');
            r.push_back(a[i]);
        }
    }
    r.push_back(L'"');
    return r;
}

}  // namespace

namespace {
UINT g_oldCodePage = 0;
void restoreCodePage() {
    if (g_oldCodePage) SetConsoleOutputCP(g_oldCodePage);
}
}  // namespace

// cmd usa por omisión la página de códigos 850; los programas escriben UTF-8.
void initConsole() {
    const UINT old = GetConsoleOutputCP();  // 0 si no hay consola (salida redirigida)
    if (old != 0 && old != CP_UTF8 && SetConsoleOutputCP(CP_UTF8)) {
        g_oldCodePage = old;
        std::atexit(restoreCodePage);
    }
}

int currentPid() { return static_cast<int>(GetCurrentProcessId()); }

bool pidAlive(int pid) {
    if (pid <= 0) return false;
    HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!h) return GetLastError() == ERROR_ACCESS_DENIED;  // existe pero no tenemos permiso
    DWORD r = WaitForSingleObject(h, 0);
    CloseHandle(h);
    return r == WAIT_TIMEOUT;  // si ya terminó, el handle está señalado
}

void termInstall() {
    g_termEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    SetConsoleCtrlHandler(termHandler, TRUE);
}

bool termWait(int ms) {
    if (!g_termEvent) return false;
    return WaitForSingleObject(g_termEvent, static_cast<DWORD>(ms)) == WAIT_OBJECT_0;
}

void ignoreBrokenPipe() {}  // Windows no tiene SIGPIPE

std::string selfDir() {
    wchar_t buf[MAX_PATH * 4];
    DWORD n = GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(sizeof(buf) / sizeof(buf[0])));
    if (n == 0) return ".\\";
    std::wstring w(buf, n);
    size_t slash = w.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return ".\\";
    w = w.substr(0, slash + 1);
    // UTF-16 -> UTF-8
    int len = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), &out[0], len, nullptr, nullptr);
    return out;
}

int runProgram(const std::string& path, const std::vector<std::string>& args) {
    std::wstring cmd = quoteArg(widen(path));
    for (const std::string& a : args) cmd += L" " + quoteArg(widen(a));

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    // bInheritHandles = TRUE: el programa real usa la misma entrada y salida estándar
    // (redirecciones, tuberías) que el wrapper.
    if (!CreateProcessW(nullptr, &cmd[0], nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) return -1;

    SetConsoleCtrlHandler(swallowHandler, TRUE);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return static_cast<int>(code);
}

}  // namespace plat
