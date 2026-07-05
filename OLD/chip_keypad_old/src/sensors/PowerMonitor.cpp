#include "sensors/PowerMonitor.h"

#include <Arduino.h>

#include "core/PeripheralState.h"

namespace PowerMonitor
{
    void begin()
    {
        pinMode(PeripheralConfig::CURRENT_SENSOR_PIN, INPUT_ANALOG);
        pinMode(PeripheralConfig::VOLTAGE_SENSOR_PIN, INPUT_ANALOG);
        analogReadResolution(12);
        update();
    }

    void update()
    {
        uint32_t currentTotal = 0;
        uint32_t voltageTotal = 0;
        for (uint8_t i = 0; i < PeripheralConfig::POWER_MONITOR_SAMPLES; ++i)
        {
            currentTotal += analogRead(PeripheralConfig::CURRENT_SENSOR_PIN);
            voltageTotal += analogRead(PeripheralConfig::VOLTAGE_SENSOR_PIN);
        }

        State.currentRawAdc = static_cast<uint16_t>(currentTotal / PeripheralConfig::POWER_MONITOR_SAMPLES);
        const int32_t sensorMv = (static_cast<int32_t>(State.currentRawAdc) * PeripheralConfig::ADC_REFERENCE_MV) / PeripheralConfig::ADC_MAX;
        const int32_t currentMa = ((sensorMv - PeripheralConfig::ACS723_ZERO_MV) * 1000L) / PeripheralConfig::ACS723_MV_PER_AMP;
        State.currentMa = static_cast<int16_t>(constrain(currentMa, -32768, 32767));

        State.voltageRawAdc = static_cast<uint16_t>(voltageTotal / PeripheralConfig::POWER_MONITOR_SAMPLES);
        const uint32_t voltageAdcMv = (static_cast<uint32_t>(State.voltageRawAdc) * PeripheralConfig::ADC_REFERENCE_MV) / PeripheralConfig::ADC_MAX;
        const uint32_t voltageTopMv = (voltageAdcMv * (PeripheralConfig::VOLTAGE_DIVIDER_RA_OHMS + PeripheralConfig::VOLTAGE_DIVIDER_RB_OHMS)) / PeripheralConfig::VOLTAGE_DIVIDER_RB_OHMS;
        State.supplyMv = static_cast<uint16_t>(constrain(voltageTopMv, 0UL, 65535UL));
    }
}
