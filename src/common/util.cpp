#include "util.h"

#include <cctype>
#include <chrono>
#include <cstdlib>
#include <sstream>

#include "shared.h"

namespace ev {

bool parsePositive(const std::string& s, long& out) {
    if (s.empty()) return false;
    for (char c : s)
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    if (s.size() > 9) return false;  // evita desbordar
    out = std::strtol(s.c_str(), nullptr, 10);
    return out > 0;
}

std::string getEnv(const char* name) {
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
}

int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
}

std::string trim(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::vector<std::string> splitWs(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream is(s);
    std::string w;
    while (is >> w) out.push_back(w);
    return out;
}

std::string attachMessage(int rc, const std::string& name) {
    switch (rc) {
        case kNoSystem:
            return "el sistema '" + name + "' no existe (ejecute primero: evaluator init)";
        case kOsError:
            return "no se pudo abrir el sistema '" + name + "': segmento inválido o incompatible";
        case kTerminated:
            return "el sistema '" + name + "' está terminando";
        default:
            return "error desconocido";
    }
}

}  // namespace ev
