# DeskBridge

DeskBridge es un proyecto modular de hardware y software para crear un centro
de control de escritorio. El sistema combina un firmware principal, un
periferico auxiliar, una aplicacion de PC, una interfaz movil y protocolos de
comunicacion para supervisar sensores, controles fisicos, iluminacion,
notificaciones y estado del dispositivo.

El objetivo es construir una base ordenada que pueda evolucionar desde
prototipo hasta un producto propio, separando responsabilidades entre chips,
programas y librerias compartidas.

## Estado Actual

El repositorio contiene:

- `chip_core`: firmware principal del sistema.
- `chip_keypad`: firmware del periferico auxiliar de botones, acciones y luz.
- `pc_app`: programa de PC con CLI, TUI, GUI web local y backend residente.
- `mobil_app`: base de app movil Expo / React Native con conexion BLE.
- `shared_lib`: librerias compartidas entre firmwares.
- `docs`: documentacion tecnica auxiliar.
- `Diagrams`: diagramas de distribucion del hardware.
- `OLD`: copias historicas de la estructura anterior.

## Funciones Principales

- Comunicacion PC <-> CHIP_CORE por USB CDC.
- Comunicacion CHIP_CORE <-> CHIP_KEYPAD por UART.
- Protocolo compartido para el periferico keypad.
- Lectura y visualizacion de sensores ambientales.
- Control de iluminacion y estados de luz.
- Botones KEY programables para acciones de teclado/multimedia.
- Notificaciones `[TFI]`, eventos `[EVT]`, errores `[ERR]` y datos `[DAT]`.
- GUI local basada en WebView.
- TUI de diagnostico para pruebas fisicas.
- Backend de PC con integracion de notificaciones en Windows.
- App movil base por BLE para consultar firmware, sensores y light.
- Scripts auxiliares para limpiar y programar STM32 por STLINK.

## Arquitectura

```text
PC APP / GUI / TUI / Backend
        |
        | USB CDC
        v
CHIP_CORE
        |
        | UART
        v
CHIP_KEYPAD
```

El `CHIP_CORE` coordina el sistema, expone el protocolo hacia el PC y concentra
el estado del dispositivo. El `CHIP_KEYPAD` descarga tareas fisicas como
botones, acciones y control auxiliar de iluminacion. La app movil usa BLE como
canal reducido de consulta/control.

## Versiones e Identificadores

Valores actuales de referencia:

- Firmware CORE: `0.4.0`
- Protocolo USB/serial shell: `0x04`
- USB VID/PID: `1209:DB01`
- BLE device name: `DBx01`
- BLE service UUID: `8f7f0001-6b21-4f5e-9c7e-123456780001`
- BLE characteristic UUID: `8f7f0002-6b21-4f5e-9c7e-123456780001`
- Protocolo CHIP_KEYPAD: `TXT1`

## Programa de PC

La aplicacion de PC vive en `pc_app`.

Entradas principales:

```powershell
cd U:\DeskBridge\pc_app
python terminal.py --cli
python terminal.py --tui
python terminal.py --gui
```

El programa incluye:

- CLI para comandos directos.
- TUI para monitorizacion durante pruebas.
- GUI local WebView.
- Backend residente con icono y notificaciones.
- Configuracion persistente en `pc_app/data/data.config`.
- Idiomas en `pc_app/data/settings/language`.
- Paletas en `pc_app/data/settings/palete`.

## Firmwares

Los firmwares se compilan con PlatformIO.

CORE:

```powershell
cd U:\DeskBridge\chip_core
pio run -e esp32-s3-devkitc-uart
```

KEYPAD:

```powershell
cd U:\DeskBridge\chip_keypad
pio run -e genericSTM32G030F6
```

Scripts auxiliares del repositorio:

- `stm32Clear.cmd`: borra la memoria del STM32 con STLINK.
- `stm32Programmer.cmd`: programa un firmware por STLINK indicando carpeta.

## App Movil

La app movil vive en `mobil_app` y usa Expo / React Native.

```powershell
cd U:\DeskBridge\mobil_app
npm install
npm run start
```

Para BLE real no sirve Expo Go; hace falta una build de desarrollo o una build
nativa porque se usa `react-native-ble-plx`.

```powershell
npx expo prebuild
npx expo run:android
```

La app movil consulta:

- Version del firmware.
- Estado de sensores.
- Estado/control de light.
- Estado basico de conexion BLE.

## Verificacion Rapida

Comandos usados como comprobacion general del proyecto:

```powershell
python -m compileall U:\DeskBridge\pc_app
cd U:\DeskBridge\chip_core
pio run -e esp32-s3-devkitc-uart
cd U:\DeskBridge\chip_keypad
pio run -e genericSTM32G030F6
cd U:\DeskBridge\mobil_app
npx expo-doctor
```

## Notas

Este proyecto esta en desarrollo activo. Algunas partes estan pensadas para
pruebas fisicas y pueden cambiar segun la distribucion final del hardware.

La carpeta `OLD` contiene copias historicas y no representa la ruta activa de
desarrollo. La ruta activa es `chip_core`, `chip_keypad`, `pc_app`,
`mobil_app` y `shared_lib`.
