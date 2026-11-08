// Utilidades pequeñas compartidas por todos los programas.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ev {

// Entero estrictamente positivo. false si el texto no lo es.
bool parsePositive(const std::string& s, long& out);
// Valor de una variable de ambiente, o "" si no existe.
std::string getEnv(const char* name);
// Milisegundos desde epoch.
int64_t nowMs();
std::string trim(const std::string& s);
std::vector<std::string> splitWs(const std::string& s);
// Texto para un error de attach() (códigos 2, 3, 5).
std::string attachMessage(int rc, const std::string& name);

}  // namespace ev
