# DeskBridge Mobile

Base de aplicacion movil para visualizar DeskBridge desde Expo / React Native.

## Estado actual

- Muestra version de firmware, protocolo y VID/PID.
- Escanea el servicio BLE `8f7f0001-6b21-4f5e-9c7e-123456780001`.
- Se conecta al dispositivo `DBx01` y se suscribe a notificaciones.
- Parsea respuestas `[INF]`, `[DAT]` y `[ERR]` del protocolo BLE de `chip_core/wireless_ble.txt`.
- Muestra tarjetas de sensores ambientales y calidad de aire desde la telemetria BLE.
- Incluye controles de luz: activacion, brillo, kelvin y modo.
- Usa una estetica equivalente a la GUI del PC: fondo oscuro, paneles compactos, azul de accion y verde de estado.
- Usa datos simulados solo si el modulo BLE nativo no esta disponible.

## Requisito pendiente en este PC

VS Code tiene extensiones para Expo y React Native, pero en esta terminal no esta disponible Node/npm.

Instala Node.js LTS y despues ejecuta:

```powershell
cd U:\DeskBridge\mobil_app
npm install
```

BLE no funciona en Expo Go porque `react-native-ble-plx` necesita codigo nativo. Usa una build de desarrollo o una build nativa:

```powershell
npx expo prebuild
npx expo run:android
```

Despues, para arrancar Metro:

```powershell
npm run start
npm run android
```

## Protocolo BLE

La app escribe comandos de texto plano en la caracteristica BLE:

- `info?`, `sensors?`, `config?`, `light?`
- `light enabled on|off`
- `light brightness <0-65535>`
- `light kelvin <0-65535>`
- `light mode one|two|composite`
