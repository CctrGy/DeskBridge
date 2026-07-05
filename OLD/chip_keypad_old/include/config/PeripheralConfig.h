#pragma once

#include <Arduino.h>

#include "pins_config.h"

namespace PeripheralConfig
{
    constexpr uint32_t UART_BAUD = 115200;
    constexpr uint32_t UART_RX_PIN = PeripheralPins::UART_RX;
    constexpr uint32_t UART_TX_PIN = PeripheralPins::UART_TX;

    constexpr uint8_t PWM_COLD_PIN = PeripheralPins::PWM_COLD;
    constexpr uint8_t PWM_WARM_PIN = PeripheralPins::PWM_WARM;
    constexpr uint32_t PWM_FREQUENCY_HZ = 1600;
    constexpr uint8_t PWM_RESOLUTION_BITS = 16;
    constexpr uint16_t PWM_MAX = 65535;

    constexpr uint8_t BUTTON_POWER_PIN = PeripheralPins::BUTTON_POWER;
    constexpr uint8_t BUTTON_ADJUST_PIN = PeripheralPins::BUTTON_ADJUST;
    constexpr uint8_t BUTTON_COUNT = 5;
    constexpr uint8_t BUTTON_PINS[BUTTON_COUNT] = {
        PeripheralPins::BUTTON_0,
        PeripheralPins::BUTTON_1,
        PeripheralPins::BUTTON_2,
        PeripheralPins::BUTTON_3,
        PeripheralPins::BUTTON_4,
    };
    constexpr uint8_t EVENT_OUT_PIN = PeripheralPins::EVENT_OUT;
    constexpr uint8_t STATE_OUT_PIN = PeripheralPins::STATE_OUT;

    constexpr uint8_t CURRENT_SENSOR_PIN = PeripheralPins::CURRENT_SENSOR;
    constexpr uint8_t VOLTAGE_SENSOR_PIN = PeripheralPins::VOLTAGE_SENSOR;
    constexpr uint16_t ADC_MAX = 4095;
    constexpr uint16_t ADC_REFERENCE_MV = 3300;
    constexpr uint16_t ACS723_ZERO_MV = ADC_REFERENCE_MV / 2;
    constexpr uint16_t ACS723_MV_PER_AMP = 200;
    constexpr uint8_t POWER_MONITOR_SAMPLES = 8;
    constexpr uint32_t VOLTAGE_DIVIDER_RA_OHMS = 100000;
    constexpr uint32_t VOLTAGE_DIVIDER_RB_OHMS = 12000;
    constexpr uint16_t VOLTAGE_ADC_MAX_MV = ADC_REFERENCE_MV;
    constexpr uint16_t VOLTAGE_TOP_MAX_MV = static_cast<uint16_t>((static_cast<uint32_t>(VOLTAGE_ADC_MAX_MV) * (VOLTAGE_DIVIDER_RA_OHMS + VOLTAGE_DIVIDER_RB_OHMS)) / VOLTAGE_DIVIDER_RB_OHMS);

    constexpr uint16_t DEFAULT_SUPPLY_MV = 24000;
    constexpr uint16_t DEFAULT_BRIGHTNESS = PWM_MAX / 4;
    constexpr uint16_t DEFAULT_KELVIN = PWM_MAX / 2;
    constexpr uint16_t DEFAULT_STEP = 2048;
    constexpr uint16_t DEFAULT_DEBOUNCE_MS = 35;
    constexpr uint16_t DEFAULT_CLICK_MS = 600;
    constexpr uint16_t DEFAULT_LONG_MS = 600;
    constexpr uint16_t DEFAULT_REPEAT_MS = 40;
}
