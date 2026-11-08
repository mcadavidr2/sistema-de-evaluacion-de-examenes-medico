// eval-rep: consume y muestra reportes de la cola de salida (consumidor).
//
//   evaluator rep [-s <nombre>] { -i <segundos> | -m <cantidad> }
#include <chrono>
#include <iostream>

#include "../common/shared.h"
#include "../common/util.h"

using namespace ev;
using namespace std::chrono;

namespace {
void usage() { std::cerr << "uso: evaluator rep [-s <nombre>] { -i <segundos> | -m <cantidad> }\n"; }

void print(const Slot& s) {
    std::cout << "[" << s.id << " " << static_cast<unsigned>(s.inQueue) << " " << s.type << " " << s.result << "]\n";
    std::cout.flush();
}
}  // namespace

int main(int argc, char** argv) {
    plat::initConsole();
    std::string name = getEnv("EVAL_SHM_NAME");
    if (name.empty()) name = "evaluator";
    long seconds = 0, count = 0;
    for (int a = 1; a < argc; ++a) {
        std::string opt = argv[a];
        if ((opt == "-s" || opt == "-i" || opt == "-m") && a + 1 < argc) {
            std::string val = argv[++a];
            if (opt == "-s") {
                name = val;
            } else if (!parsePositive(val, opt == "-i" ? seconds : count)) {
                std::cerr << "error: " << opt << " requiere un entero mayor que cero\n";
                return kUsage;
            }
        } else {
            usage();
            return kUsage;
        }
    }
    if ((seconds > 0) == (count > 0)) {  // son excluyentes y una de las dos es obligatoria
        usage();
        return kUsage;
    }

    plat::ignoreBrokenPipe();
    Shared sh;
    if (int rc = sh.attach(name); rc != kOk) {
        std::cerr << "error: " << attachMessage(rc, name) << "\n";
        return rc;
    }

    try {
        Slot s;
        if (seconds > 0) {
            // -i: consume los reportes que lleguen durante `seconds` segundos.
            const auto deadline = steady_clock::now() + std::chrono::seconds(seconds);
            for (;;) {
                long long left = duration_cast<milliseconds>(deadline - steady_clock::now()).count();
                if (left <= 0) return kOk;
                QStatus st = sh.popOut(s, static_cast<int>(left));
                if (st == QStatus::Timeout) return kOk;
                if (st != QStatus::Ok) break;
                print(s);
            }
        } else {
            // -m: consume exactamente `count` reportes, esperando hasta completarlos.
            for (long n = 0; n < count; ++n) {
                QStatus st = sh.popOut(s, -1);
                if (st != QStatus::Ok) {
                    std::cerr << "el sistema terminó antes de completar los reportes (" << n << " de " << count << ")\n";
                    return kTerminated;
                }
                print(s);
            }
            return kOk;
        }
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return kOsError;
    }
    std::cerr << "el sistema terminó mientras se esperaban reportes\n";
    return kTerminated;
}
