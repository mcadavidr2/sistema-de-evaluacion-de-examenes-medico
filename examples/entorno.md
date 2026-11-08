# Configuraciones de ejemplo

```
# Rápido, silencioso
EVAL_TIME_SCALE=20 EVAL_LOG=0 evaluator init

# Poco reactivo y robot lento: se ve el trabajo del robot
evaluator init -b 10 -d 10 -s 10 -q 2

# Salida pequeña: los analizadores se bloquean hasta que rep consuma
evaluator init -oe 2

# Varios sistemas a la vez, cada uno con su nombre
evaluator init -n sistemaA
evaluator reg -n sistemaA examples/muestras1.txt
```
