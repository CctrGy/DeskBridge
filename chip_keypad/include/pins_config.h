#pragma once

#include <Arduino.h>

namespace KeypadConfig {

constexpr uint8_t BUTTON_COUNT = 5;

constexpr uint32_t UART_RX_PIN = PA2;
constexpr uint32_t UART_TX_PIN = PA3;

constexpr uint32_t EVENT_PIN = PA1;
constexpr uint32_t STATE_PIN = PB0;

constexpr uint32_t BUTTON_PINS[BUTTON_COUNT] = {
  PB3,
  PA12,
  PA11,
  PA7,
  PA6,
};

constexpr bool BUTTON_ACTIVE_LOW = false;
constexpr bool BUTTON_PULLUP = false;

constexpr unsigned long DEBOUNCE_MS = 40;
constexpr unsigned long CLICK_MS = 250;
constexpr unsigned long LONG_PRESS_MS = 700;
constexpr unsigned long TICK_INTERVAL_MS = 1;

constexpr unsigned long CORE_UART_BAUD = 115200;
constexpr uint8_t UART_LINE_CAPACITY = 48;

}  // namespace KeypadConfig
