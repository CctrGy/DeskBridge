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
#define OLED_MOSI_PIN PA7
#define OLED_CLK_PIN PA5

// Front-panel controls.
#define ENC_CLK_PIN PB12
#define ENC_DT_PIN PB13
#define ENC_BTN_PIN PB14
#define BTN_ESC_PIN PB15

// System I2C bus: RTC + EEPROM.
#define I2C_SYSTEM_SDA_PIN PB7
#define I2C_SYSTEM_SCL_PIN PB6

// Sensor I2C bus.
#define I2C_SENSOR_SDA_PIN PB8
#define I2C_SENSOR_SCL_PIN PA8
#define I2C_SENSOR_SDA_PIN_NAME PB_8
#define I2C_SENSOR_SCL_PIN_NAME PA_8
