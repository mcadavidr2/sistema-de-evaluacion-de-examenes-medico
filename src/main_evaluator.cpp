// evaluator: wrapper. Es el único programa que invoca el usuario; lee el primer argumento
// (init, reg, ctrl, rep) y ejecuta el programa real eval-<comando> con el resto de argumentos.
// Devuelve el código de salida del programa real.
//   Linux:   execv (el wrapper se reemplaza por el programa real)
//   Windows: CreateProcess, espera y devuelve el código con GetExitCodeProcess
#include <iostream>
#include <string>
#include <vector>

#include "platform/platform.h"

static void usage() {
    std::cerr << "uso: evaluator <comando> [opciones]\n"
                 "  init [-i <n>] [-ie <n>] [-oe <n>] [-n <nombre>] [-b <n>] [-d <n>] [-s <n>] [-q <n>] [-ee <seg>]\n"
                 "  reg  [-n <nombre>] { <archivo> ... | - }\n"
                 "  ctrl [-s <nombre>]\n"
                 "  rep  [-s <nombre>] { -i <segundos> | -m <cantidad> }\n";
}

int main(int argc, char** argv) {
    plat::initConsole();
    if (argc < 2) {
        usage();
        return 1;
    }
    const std::string cmd = argv[1];
    if (cmd != "init" && cmd != "reg" && cmd != "ctrl" && cmd != "rep") {
        std::cerr << "error: comando desconocido '" << cmd << "'\n";
        usage();
        return 1;
    }
    // Los programas reales están junto al wrapper, sin importar desde qué directorio se invoque.
    const std::string path = plat::selfDir() + "eval-" + cmd + plat::kExeSuffix;
    const std::vector<std::string> args(argv + 2, argv + argc);
    const int rc = plat::runProgram(path, args);
    if (rc < 0) {
        std::cerr << "error: no se pudo ejecutar " << path << "\n";
        return 3;
    }
    return rc;
}
