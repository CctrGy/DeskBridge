#pragma once

#include <Arduino.h>

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

// USB state pins.
#define USB_VBUS_PIN PA9
#define USB_ID_PIN PA10

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

// UART keypad controller.
#define KEYPAD_UART_CONFIGURED 1
#define KEYPAD_UART_RX_PIN PB7
#define KEYPAD_UART_TX_PIN PB6
#define KEYPAD_RESET_PIN PB4
#define KEYPAD_EVENT_PIN PB5
#define KEYPAD_STATE_PIN PA8
