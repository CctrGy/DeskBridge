# DeskBridgeCore

Local adapter library for the DeskBridge firmware.

The classes in `src/` are intentionally thin wrappers over the firmware modules.
They provide a cleaner object-oriented API without moving the low-level
implementation out of the firmware source tree.

- `HardwareTask`: main runtime task, loop counters and loop timing.
- `USB`: USB facade. Intended API: `USB.getCom()` and `USB.getKeyboard()`.
- `Crono`: small millis-based timer helper.
- `LightStrip`: dual-white strip adapter.
- `LedNotifications`: 3-NeoPixel notification adapter.
- `Notifications`: notification state and LED routing adapter.
- `Controls`: encoder, encoder button and ESC adapter.
- `Bus`: I2C bus and sensor adapter.
- `OledDisplay`: U8g2 display adapter.
- `OledMenu`: interactive OLED menu adapter.
- `OledWindow`: base object for drawn OLED screens.
- `Window`: simple screen layout helper on top of `OledDisplay`.
- `EStore`: pointer-based EEPROM cell/block adapter.
