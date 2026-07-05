#pragma once

#include <Arduino.h>

#ifndef NC
    #define NC -1
#endif

#if defined(DESKBRIDGE_TARGET_ESP32S3) || defined(ARDUINO_ARCH_ESP32)

// ESP32-S3 DevKitC assignment.
#define STATUS_LED_PIN NC

// NeoPixel notification strip.
#define NEOPIXEL_PIN 41
#define NEOPIXEL_COUNT 3

// OLED SPI display.
#define OLED_CS_PIN 8
#define OLED_DC_PIN 18
#define OLED_RST_PIN 17
#define OLED_MOSI_PIN 16
#define OLED_MISO_PIN NC
#define OLED_CLK_PIN 15

// Front-panel controls.
#define BUILTIN_KEY_PIN NC
#define ENC_CLK_PIN 37
#define ENC_DT_PIN 38
#define ENC_BTN_PIN 39
#define BTN_ESC_PIN 40

// USB state pins are not used on the ESP32-S3 prototype.
#define USB_VBUS_PIN NC
#define USB_ID_PIN NC

// Shared physical I2C bus for system devices and sensors.
#define I2C_SYSTEM_SDA_PIN 35
#define I2C_SYSTEM_SCL_PIN 36
#define I2C_SYSTEM_SDA_PIN_NAME I2C_SYSTEM_SDA_PIN
#define I2C_SYSTEM_SCL_PIN_NAME I2C_SYSTEM_SCL_PIN

// Logical sensor bus alias.
#define I2C_SENSOR_SDA_PIN I2C_SYSTEM_SDA_PIN
#define I2C_SENSOR_SCL_PIN I2C_SYSTEM_SCL_PIN
#define I2C_SENSOR_SDA_PIN_NAME I2C_SYSTEM_SDA_PIN_NAME
#define I2C_SENSOR_SCL_PIN_NAME I2C_SYSTEM_SCL_PIN_NAME

// CHIP_KEYPAD is not wired in the current ESP32-S3 prototype.
#define KEYPAD_UART_TX_PIN NC
#define KEYPAD_UART_RX_PIN NC
#define KEYPAD_EVENT_PIN NC
#define KEYPAD_STATE_PIN NC
#define KEYPAD_RESET_PIN NC

#else

// On-board status LED.
#define STATUS_LED_PIN PC13

// Three-pixel WS2812/NeoPixel indicator.
#define NEOPIXEL_PIN PA1
#define NEOPIXEL_COUNT 3

// OLED SPI display.
#define OLED_CS_PIN PA4
#define OLED_DC_PIN PB0
#define OLED_RST_PIN PB1
#define OLED_MOSI_PIN PA7 // SDA
#define OLED_CLK_PIN PA5

// Front-panel controls.
#define BUILTIN_KEY_PIN PA0
#define ENC_CLK_PIN PB12
#define ENC_DT_PIN PB13
#define ENC_BTN_PIN PB14
#define BTN_ESC_PIN PB15

// USB state pins are disabled because PA9/PA10 are used by CHIP_KEYPAD.
#define USB_VBUS_PIN NC
#define USB_ID_PIN NC

// Shared physical I2C bus. The firmware still exposes System and Sensors as
// separate logical buses, but both use I2C1 on PB9/PB8.
#define I2C_SYSTEM_SDA_PIN PB9
#define I2C_SYSTEM_SCL_PIN PB8
#define I2C_SYSTEM_SDA_PIN_NAME PB_9
#define I2C_SYSTEM_SCL_PIN_NAME PB_8

// Logical sensor bus alias.
#define I2C_SENSOR_SDA_PIN I2C_SYSTEM_SDA_PIN
#define I2C_SENSOR_SCL_PIN I2C_SYSTEM_SCL_PIN
#define I2C_SENSOR_SDA_PIN_NAME I2C_SYSTEM_SDA_PIN_NAME
#define I2C_SENSOR_SCL_PIN_NAME I2C_SYSTEM_SCL_PIN_NAME

// Keypad controller: UART2 + EVENT/STATE/RST.
// STM32 UART2 standard mapping: PA2=TX, PA3=RX.
#define KEYPAD_UART_TX_PIN PA2
#define KEYPAD_UART_RX_PIN PA3
#define KEYPAD_EVENT_PIN PA9
#define KEYPAD_STATE_PIN PA8
#define KEYPAD_RESET_PIN PA10

#endif
