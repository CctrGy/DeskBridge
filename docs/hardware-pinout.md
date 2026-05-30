# DeskBridge STM32F401 pinout

## Strip dual white

| Signal | Pin |
| --- | --- |
| Cold white PWM | PB8 |
| Warm white PWM | PB9 |
| Brightness button | PC14 |
| Kelvin button | PC15 |

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
| SDA | PB7 |
| SCL | PB6 |

The shared I2C bus is intended for DS3231, AT24C32, ENS160+AHT21, VEML7700,
SCD41, and SHT21 modules.
