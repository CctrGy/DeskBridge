#pragma once

#include <Arduino.h>

// On-board status LED.
#define STATUS_LED_PIN PC13

// Dual-white strip: cold white + warm white.
#define STRIP_COLD_WHITE_PIN PB8
#define STRIP_WARM_WHITE_PIN PB9
#define STRIP_BRIGHTNESS_BUTTON_PIN PC14
#define STRIP_KELVIN_BUTTON_PIN PC15

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
#define ENC_CLK_PIN PB13
#define ENC_DT_PIN PB14
#define ENC_BTN_PIN PB12
#define BTN_ESC_PIN PB15

// Shared I2C bus.
#define I2C_SDA_PIN PB7
#define I2C_SCL_PIN PB6
