#!/usr/bin/env bash
# Plan de pruebas (ver doc/Proyecto-pruebas-y-trabajo.md). Se ejecuta desde la raíz del proyecto:
#   make test            (Linux)
#   bash tests/run_tests.sh   (Linux, o Windows desde la consola MSYS2 UCRT64)
#
# Las pruebas 8 y 10 envían señales a init; solo se automatizan en Linux. En Windows se hacen
# a mano (Ctrl+C en la consola de init; terminar el proceso en el Administrador de tareas).
cd "$(dirname "$0")/.." || exit 1

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*) WINDOWS=1; EXE=.exe ;;
    *) WINDOWS=0; EXE= ;;
esac
EV="bin/evaluator$EXE"
[ -x "$EV" ] || { echo "falta $EV: ejecute make primero"; exit 1; }

# Las pruebas no deben depender del ambiente de quien las ejecuta.
unset EVAL_SHM_NAME EVAL_MAX_WAIT EVAL_ROBOT_CAPACITY
export EVAL_LOG=1   # algunas comprobaciones leen el registro de init
export EVAL_TIME_SCALE=${EVAL_TIME_SCALE:-20}
TMP=$(mktemp -d)
PASS=0; FAIL=0; N=0
INIT_PIDS=()

cleanup() {
    for p in "${INIT_PIDS[@]}"; do kill -9 "$p" 2>/dev/null; done
    # Linux: los init que se mataron a la fuerza dejan su segmento en /dev/shm
    [ "$WINDOWS" -eq 0 ] && rm -f /dev/shm/t$$_*
    rm -rf "$TMP"
}
trap cleanup EXIT

ok()   { PASS=$((PASS+1)); echo "  ok   - $1"; }
bad()  { FAIL=$((FAIL+1)); echo "  FALLA - $1"; }
check() { if eval "$2"; then ok "$1"; else bad "$1"; fi; }

# start_init <nombre> [opciones de init...]: lanza init y espera a que el sistema esté listo.
start_init() {
    local name=$1; shift
    "$EV" init -n "$name" "$@" > "$TMP/$name.log" 2>&1 &
    INIT_PIDS+=($!)
    LAST_PID=$!
    for _ in $(seq 1 100); do
        "$EV" ctrl -s "$name" < /dev/null > /dev/null 2>&1 && return 0
        sleep 0.1
    done
    echo "  (init no arrancó: $(cat "$TMP/$name.log"))"
    return 1
}
stop_init() { kill -9 "$1" 2>/dev/null; wait "$1" 2>/dev/null; }
next_name() { N=$((N+1)); NAME="t$$_$N"; }

# --------------------------------------------------------------------------
echo "Prueba 1: registro básico (líneas válidas e inválidas)"
next_name; start_init "$NAME"; PID=$LAST_PID
printf '0 B 2\n9 B 1\nxyz\n1 D 3\n\n2 S 6\n' | "$EV" reg -n "$NAME" - > "$TMP/out1" 2> "$TMP/err1"
check "devuelve dos ids distintos" '[ "$(cat $TMP/out1 | sort -u | wc -l)" -eq 2 ]'
check "informa las líneas inválidas con su número" 'grep -q ":2:" $TMP/err1 && grep -q ":3:" $TMP/err1 && grep -q ":6:" $TMP/err1'
printf '0 B 1\r\n' > "$TMP/crlf.txt"
"$EV" reg -n "$NAME" "$TMP/crlf.txt" "$TMP/no-existe.txt" > /dev/null 2>&1
check "archivo CRLF: crea .spl con el id" '[ "$(tr -d "\r" < $TMP/crlf.spl)" -gt 0 ] 2>/dev/null'
check "archivo inexistente se ignora (sin .spl)" '[ ! -e $TMP/no-existe.spl ]'
"$EV" rep -s "$NAME" -m 3 > "$TMP/rep1" 2>&1
check "rep -m 3 entrega 3 reportes con formato [id i k r]" '[ "$(grep -cE "^\[[0-9]+ [0-9]+ [BDS] [PN?]\]$" $TMP/rep1)" -eq 3 ]'
stop_init $PID

# --------------------------------------------------------------------------
echo "Prueba 2: tres reg llenando una cola con -ie 2 (se bloquean y reanudan)"
next_name; start_init "$NAME" -ie 2 -i 1 -q 1 -oe 1; PID=$LAST_PID
for r in 1 2 3; do
    ( for _ in 1 2 3 4; do echo "0 B 1"; done | "$EV" reg -n "$NAME" - > "$TMP/reg2_$r" 2>&1; echo $? > "$TMP/rc2_$r" ) &
done
sleep 2
check "algún reg sigue bloqueado con la cola llena" '[ "$(ls $TMP/rc2_* 2>/dev/null | wc -l)" -lt 3 ]'
"$EV" rep -s "$NAME" -m 12 > "$TMP/rep2" 2>&1 &
REP=$!
for _ in $(seq 1 150); do [ "$(ls $TMP/rc2_* 2>/dev/null | wc -l)" -eq 3 ] && break; sleep 0.2; done
wait $REP
check "los tres reg terminan con código 0 al liberarse espacio" 'cat $TMP/rc2_* | sort -u | grep -qx 0 && [ "$(cat $TMP/rc2_* | sort -u | wc -l)" -eq 1 ]'
check "se entregaron los 12 reportes" '[ "$(wc -l < $TMP/rep2)" -eq 12 ]'
check "los 12 ids son distintos" '[ "$(cat $TMP/reg2_* | sort -u | wc -l)" -eq 12 ]'
stop_init $PID

# --------------------------------------------------------------------------
echo "Prueba 3: salida llena (-oe 2) sin rep"
next_name; start_init "$NAME" -oe 2; PID=$LAST_PID
printf '0 B 1\n0 B 1\n0 B 1\n0 B 1\n0 B 1\n0 B 1\n' | "$EV" reg -n "$NAME" - > /dev/null 2>&1
sleep 3
REPORTED=$(printf 'list reported\n' | "$EV" ctrl -s "$NAME" | grep -cE '^\[')
check "en la salida hay exactamente 2 reportes (los analizadores se bloquearon)" '[ "$REPORTED" -eq 2 ]'
"$EV" rep -s "$NAME" -m 6 > "$TMP/rep3" 2>&1
check "al consumir con rep se reanudan y salen los 6" '[ "$(wc -l < $TMP/rep3)" -eq 6 ]'
stop_init $PID

# --------------------------------------------------------------------------
echo "Prueba 4: reactivo (-b 5) con muestras grandes"
next_name; start_init "$NAME" -b 5; PID=$LAST_PID
INIT_REACTIVE=$(printf 'list reactive\n' | "$EV" ctrl -s "$NAME" | grep -E '^B ')
check "list reactive muestra B 5 al inicio" '[ "$INIT_REACTIVE" = "B 5" ] || echo "$INIT_REACTIVE" | grep -q "B 5"'
printf '0 B 5\n0 B 5\n0 B 5\n' | "$EV" reg -n "$NAME" - > /dev/null 2>&1
"$EV" rep -s "$NAME" -m 3 > "$TMP/rep4" 2>&1
check "las 3 muestras se reportan" '[ "$(wc -l < $TMP/rep4)" -eq 3 ]'
check "el robot hizo recargas (aparece en el registro de init)" 'grep -q "robot: cargó" $TMP/$NAME.log'
stop_init $PID

# --------------------------------------------------------------------------
echo "Prueba 5: ctrl update con el analizador esperando reactivo"
next_name; EVAL_TIME_SCALE=1 start_init "$NAME" -b 5; PID=$LAST_PID
printf '0 B 5\n0 B 5\n' | "$EV" reg -n "$NAME" - > /dev/null 2>&1
for _ in $(seq 1 100); do grep -q "solicita recarga" "$TMP/$NAME.log" && break; sleep 0.2; done
check "el analizador quedó esperando reactivo" 'grep -q "solicita recarga" $TMP/$NAME.log'
UPD=$(printf 'update B 5\n' | "$EV" ctrl -s "$NAME")
check "update acepta la carga" 'echo "$UPD" | grep -q "Reactivo B"'
"$EV" rep -s "$NAME" -m 2 > "$TMP/rep5" 2>&1 &
REP=$!
for _ in $(seq 1 150); do kill -0 $REP 2>/dev/null || break; sleep 0.2; done
if kill -0 $REP 2>/dev/null; then kill $REP; bad "el analizador continuó tras el update"; else ok "el analizador continuó tras el update"; fi
stop_init $PID

# --------------------------------------------------------------------------
echo "Prueba 6: tiempo máximo de espera (-ee 1)"
next_name; EVAL_TIME_SCALE=4 start_init "$NAME" -ee 1 -q 6; PID=$LAST_PID
printf '0 S 1\n0 S 1\n0 S 1\n0 S 1\n0 S 1\n0 S 1\n' | "$EV" reg -n "$NAME" - > /dev/null 2>&1
"$EV" rep -s "$NAME" -m 6 > "$TMP/rep6" 2>&1
check "las muestras que esperan de más salen con ?" '[ "$(grep -c " ?\]" $TMP/rep6)" -ge 3 ]'
stop_init $PID

# --------------------------------------------------------------------------
echo "Prueba 7: rep -i y rep -m"
next_name; start_init "$NAME"; PID=$LAST_PID
T0=$(date +%s)
"$EV" rep -s "$NAME" -i 2 > /dev/null 2>&1; RC=$?
T1=$(date +%s)
check "rep -i 2 termina por tiempo con código 0" '[ $RC -eq 0 ] && [ $((T1-T0)) -ge 1 ] && [ $((T1-T0)) -le 4 ]'
"$EV" rep -s "$NAME" -m 1 > "$TMP/rep7" 2>&1 &
REP=$!
sleep 1
check "rep -m 1 espera mientras no hay reportes" 'kill -0 $REP 2>/dev/null'
echo "0 B 1" | "$EV" reg -n "$NAME" - > /dev/null 2>&1
for _ in $(seq 1 100); do kill -0 $REP 2>/dev/null || break; sleep 0.2; done
check "rep -m 1 termina cuando llega el reporte" '! kill -0 $REP 2>/dev/null && [ "$(wc -l < $TMP/rep7)" -eq 1 ]'
kill $REP 2>/dev/null
"$EV" rep -s "$NAME" > /dev/null 2>&1; check "rep sin -i ni -m es error de uso (1)" '[ $? -eq 1 ]'
stop_init $PID

# --------------------------------------------------------------------------
echo "Prueba 8: terminación con señal (solo Linux)"
if [ $WINDOWS -eq 0 ]; then
    next_name; start_init "$NAME"; PID=$LAST_PID
    "$EV" rep -s "$NAME" -m 5 > /dev/null 2>&1 &
    REP=$!
    sleep 1
    kill -INT $PID
    for _ in $(seq 1 50); do kill -0 $PID 2>/dev/null || break; sleep 0.1; done
    check "init termina tras SIGINT" '! kill -0 $PID 2>/dev/null'
    wait $REP; RC=$?
    check "rep sale con código 5" '[ $RC -eq 5 ]'
    check "no queda segmento huérfano" '[ ! -e /dev/shm/$NAME ]'
    "$EV" ctrl -s "$NAME" < /dev/null > /dev/null 2>&1; check "ctrl contra un sistema inexistente: código 2" '[ $? -eq 2 ]'
else
    echo "  (manual en Windows: Ctrl+C en la consola de init; rep debe salir con código 5)"
fi

# --------------------------------------------------------------------------
echo "Prueba 9: init dos veces"
next_name; start_init "$NAME"; PID=$LAST_PID
"$EV" init -n "$NAME" > /dev/null 2>&1; RC=$?
check "la segunda ejecución falla con código 4" '[ $RC -eq 4 ]'
stop_init $PID

# --------------------------------------------------------------------------
echo "Prueba 10: matar init a la fuerza y volver a ejecutar (solo Linux)"
if [ $WINDOWS -eq 0 ]; then
    next_name; start_init "$NAME"; PID=$LAST_PID
    kill -9 $PID; wait $PID 2>/dev/null
    check "queda un segmento huérfano" '[ -e /dev/shm/$NAME ]'
    "$EV" ctrl -s "$NAME" < /dev/null > /dev/null 2>&1; check "los clientes lo detectan como inexistente (2)" '[ $? -eq 2 ]'
    start_init "$NAME"; PID2=$LAST_PID
    check "init limpia el residuo y arranca" 'kill -0 $PID2 2>/dev/null'
    stop_init $PID2; rm -f /dev/shm/$NAME
else
    echo "  (manual en Windows: terminar eval-init en el Administrador de tareas y volver a ejecutar init)"
fi

# --------------------------------------------------------------------------
echo "Prueba 11: varias instancias de rep"
next_name; start_init "$NAME"; PID=$LAST_PID
for _ in $(seq 1 20); do echo "$((RANDOM % 5)) B 1"; done | "$EV" reg -n "$NAME" - > /dev/null 2>&1
"$EV" rep -s "$NAME" -m 10 > "$TMP/rep11a" 2>&1 &
A=$!
"$EV" rep -s "$NAME" -m 10 > "$TMP/rep11b" 2>&1 &
B=$!
wait $A $B
check "entre las dos se reciben 20 reportes" '[ "$(cat $TMP/rep11a $TMP/rep11b | wc -l)" -eq 20 ]'
check "ningún reporte se entregó a las dos" '[ "$(cat $TMP/rep11a $TMP/rep11b | sort -u | wc -l)" -eq 20 ]'
stop_init $PID

# --------------------------------------------------------------------------
echo "Prueba 12: ctrl list all mientras el sistema trabaja"
next_name; EVAL_TIME_SCALE=2 start_init "$NAME"; PID=$LAST_PID
for _ in $(seq 1 12); do echo "$((RANDOM % 5)) D 2"; done | "$EV" reg -n "$NAME" - > /dev/null 2>&1
sleep 1
printf 'list all\nlist processing\n' | "$EV" ctrl -s "$NAME" > "$TMP/out12" 2>&1
check "se muestran los cuatro listados" 'grep -q Processing: $TMP/out12 && grep -q Waiting: $TMP/out12 && grep -q Reported: $TMP/out12 && grep -q Reactive: $TMP/out12'
check "hay muestras en proceso con formato [id i k q p]" 'grep -qE "^(> )?\[[0-9]+ [0-9]+ D [0-9]+ [0-9]+\]$" $TMP/out12'
stop_init $PID

# --------------------------------------------------------------------------
echo
echo "Resultado: $PASS ok, $FAIL con falla"
[ $FAIL -eq 0 ]
