# DeskBridge chip_keypad

Firmware del chip complementario gestionado por `chip_core`.

## Hardware

| Signal | Pin |
| --- | --- |
| UART RX | PA10 |
| UART TX | PA9 |
| UART baud | 115200 |
| Cold white MOSFET PWM | PB5 |
| Warm white MOSFET PWM | PB4 |
| Programmable buttons | PB12, PB13, PB14, PB15, PA8 |
| Event edge output to chip_core | PC13 |
| ACS723 current sensor | PA6 |
| Voltage divider ADC | PA7 |

USB no se inicializa desde el firmware. `platformio.ini` mantiene DFU para
programar el chip.

## Firmware Layout

| Path | Purpose |
| --- | --- |
| `include/config/PeripheralConfig.h` | Pines, UART y constantes globales |
| `src/core/PeripheralState.cpp` | Estado compartido y eventos |
| `src/light/StripPwm.cpp` | PWM 1.6 kHz a 16 bit para la tira LED |
| `src/input/ButtonControls.cpp` | Botones con OneButton |
| `src/sensors/PowerMonitor.cpp` | Lectura ACS723 y tension global |
| `src/bus/UARTProtocol.cpp` | Protocolo UART ASCII |

## UART

El protocolo completo esta documentado en `UART_PROTOCOL.txt`.

Resumen:

| Command | Description |
| --- | --- |
| `PING` | Comprobar enlace |
| `INF?` | Identificacion del periferico |
| `STR?` | Estado completo de tira LED |
| `PWR?` | Monitor ADC de corriente/tension |
| `EVT?` | Leer eventos pendientes |
| `BTN?` | Leer bordes PRESSED/RELEASED de botones |
| `ACT?` | Leer accion callable pendiente |
| `BAL?` | Lista de acciones disponibles para botones |
| `BAS?` | Asignaciones actuales de los 5 botones |
| `BSA0,2` | Asignar accion 2 al boton 0 |
| `SSE0` / `SSE1` | Desactivar/activar tira |
| `SSM0..2` | Modo de tira |
| `SSV32768` | Brillo |
| `SSK32768` | Mezcla Kelvin |
| `SCN`, `SCX`, `SWN`, `SWX` | Limites PWM frio/calido |
| `SBS`, `SKS` | Pasos de ajuste local |
| `SCT`, `SLT`, `SRT` | Timings de botones |

`PC13` es la salida de evento hacia `chip_core` y se usa por defecto. El CORE
lee `EVT?`/`ACT?` por UART para limpiar el evento y registrar los cambios. La
tira LED y los botones capacitivos se gestionan localmente en `chip_keypad`.

## Botones

| Button | Event | Local action |
| --- | --- | --- |
| BTN0 | Click | Toggle ON/OFF |
| BTN0 | Long press | Ajusta brillo por pasos configurados |
| BTN0 | Long release | Invierte la direccion del proximo ajuste de brillo |
| BTN1 | Click | Sin accion local |
| BTN1 | Long press | Ajusta kelvin por pasos configurados |
| BTN1 | Long release | Invierte la direccion del proximo ajuste de kelvin |

## Botones Programables

Hay 5 botones activos en HIGH y sin pullup:

```text
B0=PB12 B1=PB13 B2=PB14 B3=PB15 B4=PA8
```

Opciones iniciales:

| Id | Action |
| --- | --- |
| `0` | `Strip_PowerBrightness` |
| `1` | `Strip_WhiteColor` |
| `2` | `VolumeMuted` |
| `3` | `VolumeUp` |
| `4` | `VolumeDown` |

`BAL?` filtra la lista segun el modo de tira: en modo normal no muestra
`Strip_WhiteColor`; en modo doble blanco si la muestra. Las acciones de volumen
se devuelven como codigos hex de texto al CORE con `ACT?`.

`BTN?` devuelve que boton disparo evento fisico y si fue `PRESSED` o
`RELEASED`:

```text
OK BP=4 BR=0 BD=4 LB=2 BE=1
```

`BP` y `BR` son mascaras de botones pressed/released, `BD` es la mascara de
botones actualmente pulsados, `LB` es el ultimo boton y `BE` es `1=pressed`,
`2=released`.

## Voltimetro

El voltimetro usa un divisor `Ra=100K` y `Rb=12K`:

```text
Vadc = Vtop * 0.107142857
VtopMax = 30.8V con ADC a 3.3V
```

`PWR?` devuelve `MV` como tension calculada en milivoltios y `VRAW` como ADC
raw del pin del voltimetro.

## Amperimetro

El ACS723 configurado es el modelo bidireccional de `+/-10A`. La lectura usa:

```text
0A = Vcc / 2 = 1650mV
Sensibilidad = 200mV/A
mA = ((ADC_mV - 1650) * 1000) / 200
```
