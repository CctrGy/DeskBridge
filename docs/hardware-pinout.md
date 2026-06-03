# DeskBridge STM32F401 pinout

## Local board inputs

| Signal | Pin |
| --- | --- |
| KEY | PA0 |

## NeoPixel

| Signal | Pin |
| --- | --- |
| 3-pixel WS2812 data | PA1 |

## OLED SPI

| Signal | Pin |
| --- | --- |
| CS | PA4 |
| DC | PB0 |
| RST | PB1 |
| MOSI / SDA | PA7 |
| CLK | PA5 |

## Controls

| Signal | Pin |
| --- | --- |
| Encoder CLK | PB13 |
| Encoder DT | PB14 |
| Encoder button | PB12 |
| ESC button | PB15 |

## I2C

| Signal | Pin |
| --- | --- |
| SDA | PB9 |
| SCL | PB8 |

The shared physical I2C bus is intended for DS3231, AT24C32,
ENS160+AHT21, VEML7700, SCD41, and SHT21 modules. Firmware still keeps
`System` and `Sensors` as logical buses.

## Keypad UART

| Signal | Pin |
| --- | --- |
| UART1 RX | PB7 |
| UART1 TX | PB6 |
| Event input, unused for now | PB5 |

## USB state

| Signal | Pin |
| --- | --- |
| USB VBUS | PA9 |
| USB ID | PA10 |
