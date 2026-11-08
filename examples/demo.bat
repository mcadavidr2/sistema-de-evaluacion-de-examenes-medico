@echo off
rem Demostracion rapida en Windows (cmd). Ejecutar desde la raiz del proyecto: examples\demo.bat
set EVAL_TIME_SCALE=10
start "init" bin\evaluator.exe init -n demo
timeout /t 2 /nobreak > nul
bin\evaluator.exe reg -n demo examples\muestras1.txt examples\muestras2.txt
type examples\muestras1.spl
bin\evaluator.exe rep -s demo -m 11
echo Cierre la ventana "init" (Ctrl+C) para terminar el sistema.
