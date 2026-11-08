#pragma once

// En Windows los semáforos y mutex son objetos del kernel con nombre. Dentro del
// segmento solo se guarda el nombre, así cualquier proceso puede abrirlos.
// (Un nombre completo es "Local\" + nombre; 96 bytes alcanzan.)
struct alignas(8) PlatformSem   { char name[96]; };
struct alignas(8) PlatformMutex { char name[96]; };

namespace plat {
constexpr const char* kExeSuffix = ".exe";
constexpr char        kPathSep   = '\\';
}
