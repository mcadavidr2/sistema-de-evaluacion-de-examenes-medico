#!/usr/bin/env bash
# Demostración rápida (Linux, o MSYS2 en Windows). Ejecutar desde la raíz: bash examples/demo.sh
cd "$(dirname "$0")/.." || exit 1
EXE=; case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) EXE=.exe;; esac
EV=bin/evaluator$EXE
export EVAL_TIME_SCALE=10
$EV init -n demo &
INIT=$!
sleep 1
$EV reg -n demo examples/muestras1.txt examples/muestras2.txt
echo "--- ids asignados (examples/muestras1.spl):"; cat examples/muestras1.spl
echo "--- estado:"; printf 'list all\n' | $EV ctrl -s demo
echo "--- reportes:"; $EV rep -s demo -m 11
kill $INIT 2>/dev/null; wait $INIT 2>/dev/null
