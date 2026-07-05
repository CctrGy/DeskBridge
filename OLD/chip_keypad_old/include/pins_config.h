#pragma once

#include <Arduino.h>

namespace PeripheralPins
{
    constexpr uint32_t UART_RX = PA10;
    constexpr uint32_t UART_TX = PA9;

    constexpr uint8_t PWM_COLD = PB5;
    constexpr uint8_t PWM_WARM = PB4;

    constexpr uint8_t BUTTON_0 = PB12;
    constexpr uint8_t BUTTON_1 = PB13;
    constexpr uint8_t BUTTON_2 = PB14;
    constexpr uint8_t BUTTON_3 = PB15;
    constexpr uint8_t BUTTON_4 = PA8;

    constexpr uint8_t BUTTON_POWER = BUTTON_0;
    constexpr uint8_t BUTTON_ADJUST = BUTTON_1;

    constexpr uint8_t EVENT_OUT = PC13;
    constexpr uint8_t STATE_OUT = PC14;
    constexpr uint8_t CURRENT_SENSOR = PA6;
    constexpr uint8_t VOLTAGE_SENSOR = PA7;
}
