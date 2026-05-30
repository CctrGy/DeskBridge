# DeskBridge USB architecture

## Device model

DeskBridge exposes two physical USB functions on the STM32F401:

- one CDC ACM serial port visible to Windows as one `COMx` port
- one HID function for keyboard and consumer-control actions

The DeskBridge PC app opens the single CDC port and separates its internal
traffic into logical channels:

- `CONTROL` for commands, state sync, configuration, sensor data, and device
  management
- `LOG` for firmware logs, diagnostics, errors, and events

## STM32F401 profile

The STM32F401CC USB OTG FS peripheral exposes four bidirectional endpoint
numbers including endpoint 0.  Endpoint 0 is reserved for USB control.

A normal TinyUSB CDC ACM function uses:

- one interrupt IN endpoint for CDC notifications
- one bulk IN endpoint for data
- one bulk OUT endpoint for data

The first DeskBridge hardware profile therefore uses the available F401
endpoint numbers as:

| Endpoint | Use |
| --- | --- |
| EP0 | USB control |
| EP1 IN | CDC notification |
| EP2 IN/OUT | CDC data |
| EP3 IN | HID interrupt reports |

That profile is one physical CDC ACM function plus one HID function.  It
keeps the control path and immediate HID actions on the current BlackPill
target.

## Host view

Windows sees:

```text
DeskBridge USB Composite
|-- COMx
`-- HID
    |-- Keyboard
    `-- Consumer Control
```

The DeskBridge PC app sees:

```text
COMx
|-- CONTROL
`-- LOG
```
