# Evaluador de exámenes médicos – Diseño

**Curso:** ST0257 Sistemas Operativos · 2026-2
**Entrega 1 – Diseño (10 %)**

## Equipo

| Integrante | Sistema a cargo |
|---|---|
| Mateo Cadavid | Linux |
| Sebastian Vasquez | Windows |

## Qué pide la primera entrega y dónde está resuelto

| Requisito del enunciado | Página |
|---|---|
| Programas del sistema, línea de comandos y variables de ambiente | [Programas y línea de comandos](Programas-y-linea-de-comandos) |
| Estructura completa de la bandeja de entrada (formato de la cola y variables de control) | [Memoria compartida](Memoria-compartida) y [Bandeja de entrada](Bandeja-de-entrada) |
| Estructura completa de la bandeja de salida (formato de la cola y variables de control) | [Memoria compartida](Memoria-compartida) y [Bandeja de salida](Bandeja-de-salida) |
| Estructuras para controlar la concurrencia en cada uno de los sistemas | [Concurrencia común](Concurrencia-comun), [Concurrencia en Linux](Concurrencia-Linux) y [Concurrencia en Windows](Concurrencia-Windows) |
| Mecanismo completo de cada comando | [Mecanismo de los comandos](Mecanismo-de-los-comandos) |
| Jerarquía del proyecto, pruebas, reparto de trabajo y puntos por confirmar | [Proyecto, pruebas y reparto de trabajo](Proyecto-pruebas-y-trabajo) |

## Resumen del sistema

Se simula un sistema que evalúa muestras médicas de tres tipos: sangre (**B**), detritos (**D**) y piel (**S**). Cada resultado puede ser positivo (**P**), negativo (**N**) o no concluyente (**?**).

- `evaluator init` crea el sistema y lo ejecuta. Dentro de este proceso corren varios **hilos**.
- `evaluator reg` registra muestras en las **colas de entrada** (productor).
- `evaluator rep` consume los resultados de la **cola de salida** (consumidor).
- `evaluator ctrl` es una consola para consultar el estado y recargar reactivos.
- Los procesos se comunican mediante **memoria compartida y semáforos**.

## Decisiones generales

| Tema | Decisión |
|---|---|
| Lenguaje | C++17 (el enunciado permite C, C++, Rust, Go o Zig) |
| Sistemas operativos | Linux y Windows. Una capa `platform/` aísla lo que depende del sistema |
| Comunicación entre procesos | Memoria compartida + semáforos + mutex entre procesos |
| Concurrencia dentro de `init` | Hilos de C++ (`std::thread`) con `std::mutex` y `std::condition_variable` |
| Compilación | `Makefile` en la raíz (g++ en Linux, MinGW-w64 y `mingw32-make` en Windows) |
| Binarios | Se generan en `bin/` y no se suben al repositorio |

## Arquitectura

```
 evaluator reg (N procesos)         ┌───────────────── proceso eval-init ─────────────────┐
  ┌───────────┐                     │                                                      │
  │ registro  │──► Cola entrada 0 ──┼─► hilo entrada 0 ─┐                                  │
  │ registro  │──► Cola entrada 1 ──┼─► hilo entrada 1 ─┼─► cola interna B ─► hilo Blood ──┼─┐
  │    ...    │──► Cola entrada i-1─┼─► hilo entrada i-1┤   cola interna D ─► hilo Detr. ──┼─┤
  └───────────┘   (memoria          │                   └─► cola interna S ─► hilo Skin ───┼─┤
                   compartida)      │                                ▲                     │ │
                                    │                         hilo ROBOT ◄── solicitudes   │ │
                                    └──────────────────────────────────────────────────────┘ │
                                                                                             ▼
   evaluator rep ◄──────────────── Cola de salida (memoria compartida) ◄────────────────────┘
   evaluator ctrl ◄── lee estado / actualiza reactivo (memoria compartida)
```

Hilos dentro de `eval-init`:

- **i hilos de entrada**, uno por cola de entrada.
- **3 hilos analizadores**, uno por tipo de muestra.
- **1 hilo robot**, que recarga los reactivos.
- **1 hilo principal**, que crea el sistema, lanza los hilos, espera la señal de terminación y limpia.
