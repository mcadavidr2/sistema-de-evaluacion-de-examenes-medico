// eval-reg: registra muestras en las colas de entrada (productor).
//
//   evaluator reg [-n <nombre>] { <archivo> ... | - }
//
// Cada registro: <cola> {B|D|S} <cantidad>
//   - Con entrada estándar ("-") imprime el id de cada muestra aceptada.
//   - Con un archivo x.ext crea x.spl con una línea por cada línea del archivo: el id o ERROR.
#include <fstream>
#include <iostream>
#include <sstream>

#include "../common/shared.h"
#include "../common/util.h"

using namespace ev;

namespace {

struct Record {
    long queue;
    char type;
    long qty;
};

// Devuelve "" si la línea es válida; si no, el motivo del error.
std::string parseLine(const std::string& line, uint32_t numIn, Record& r) {
    std::istringstream is(line);
    std::string a, b, c, extra;
    if (!(is >> a >> b >> c)) return "se esperan tres campos: <cola> {B|D|S} <cantidad>";
    if (is >> extra) return "hay campos de más";
    if (a.find_first_not_of("0123456789") != std::string::npos || a.size() > 9) return "cola inválida '" + a + "'";
    r.queue = std::strtol(a.c_str(), nullptr, 10);
    if (r.queue < 0 || r.queue >= static_cast<long>(numIn))
        return "la cola debe estar entre 0 y " + std::to_string(numIn - 1);
    if (b.size() != 1 || typeIndex(b[0]) < 0) return "el tipo debe ser B, D o S";
    r.type = b[0];
    long q;
    if (!parsePositive(c, q) || q > 5) return "la cantidad debe estar entre 1 y 5";
    r.qty = q;
    return "";
}

// Procesa todas las líneas de `in`. Si `spl` no es nulo escribe allí el resultado por línea.
// Devuelve kOk o kTerminated.
int process(Shared& sh, std::istream& in, const std::string& label, std::ostream* spl, bool printIds) {
    std::string line;
    unsigned lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        if (!line.empty() && line.back() == '\r') line.pop_back();  // archivos con final CRLF (Windows)
        if (trim(line).empty()) {
            if (spl) *spl << "\n";
            continue;
        }
        Record r{};
        std::string err = parseLine(line, sh.hdr()->numIn, r);
        if (!err.empty()) {
            std::cerr << label << ":" << lineNo << ": línea inválida (" << err << "): " << line << "\n";
            if (spl) *spl << "ERROR\n";
            continue;
        }
        Slot s{};
        s.id          = sh.nextId();
        s.inQueue     = static_cast<uint8_t>(r.queue);
        s.type        = r.type;
        s.qty         = static_cast<uint8_t>(r.qty);
        s.enqueueTime = nowMs();
        // Si la cola está llena, pushIn espera hasta que se libere una posición.
        QStatus st = sh.pushIn(static_cast<uint32_t>(r.queue), s);
        if (st != QStatus::Ok) {
            std::cerr << "el sistema terminó mientras se registraban muestras\n";
            return kTerminated;
        }
        if (printIds) {
            std::cout << s.id << "\n";
            std::cout.flush();
        }
        if (spl) {
            *spl << s.id << "\n";
            spl->flush();
        }
    }
    return kOk;
}

// x.ext -> x.spl (solo se cambia la extensión del nombre, no la de un directorio)
std::string splName(const std::string& file) {
    size_t slash = file.find_last_of("/\\");
    size_t dot   = file.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return file + ".spl";
    return file.substr(0, dot) + ".spl";
}

void usage() { std::cerr << "uso: evaluator reg [-n <nombre>] { <archivo> ... | - }\n"; }

}  // namespace

int main(int argc, char** argv) {
    plat::initConsole();
    std::string name = getEnv("EVAL_SHM_NAME");
    if (name.empty()) name = "evaluator";
    std::vector<std::string> inputs;
    for (int a = 1; a < argc; ++a) {
        std::string arg = argv[a];
        if (arg == "-n") {
            if (a + 1 >= argc) {
                usage();
                return kUsage;
            }
            name = argv[++a];
        } else {
            inputs.push_back(arg);
        }
    }
    if (inputs.empty()) {
        usage();
        return kUsage;
    }

    Shared sh;
    if (int rc = sh.attach(name); rc != kOk) {
        std::cerr << "error: " << attachMessage(rc, name) << "\n";
        return rc;
    }

    try {
        for (const std::string& in : inputs) {
            int rc;
            if (in == "-") {
                rc = process(sh, std::cin, "<stdin>", nullptr, true);
            } else {
                std::ifstream f(in, std::ios::binary);
                if (!f) continue;  // un archivo que no existe se ignora, sin mensaje
                f.peek();
                if (f.bad()) continue;  // Linux abre las carpetas pero no puede leerlas: tampoco son archivos
                f.clear();
                std::ofstream spl(splName(in), std::ios::binary);  // binario: siempre termina en \n
                rc = process(sh, f, in, spl ? &spl : nullptr, false);
            }
            if (rc != kOk) return rc;
        }
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return kOsError;
    }
    return kOk;
}
