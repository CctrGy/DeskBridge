#include "light/StripPwm.h"

#include <Arduino.h>

#include "core/PeripheralState.h"

namespace
{
    uint16_t clampRange(uint16_t value, uint16_t minimum, uint16_t maximum)
    {
        if (maximum < minimum)
        {
            maximum = minimum;
        }
        return constrain(value, minimum, maximum);
    }

    uint16_t mixChannel(uint16_t value, uint16_t mix, bool coldChannel, uint16_t minimum, uint16_t maximum)
    {
        const uint16_t channelMix = coldChannel ? mix : PeripheralConfig::PWM_MAX - mix;
        const uint16_t scaled = static_cast<uint16_t>((static_cast<uint32_t>(value) * channelMix) / PeripheralConfig::PWM_MAX);
        return clampRange(scaled, minimum, maximum);
    }
}

namespace StripPwm
{
    void begin()
    {
        pinMode(PeripheralConfig::PWM_COLD_PIN, OUTPUT);
        pinMode(PeripheralConfig::PWM_WARM_PIN, OUTPUT);
        analogWriteResolution(PeripheralConfig::PWM_RESOLUTION_BITS);
        analogWriteFrequency(PeripheralConfig::PWM_FREQUENCY_HZ);
        update();
    }

    void update()
    {
        if (!State.stripEnabled)
        {
            State.pwmCold = 0;
            State.pwmWarm = 0;
        }
        else
        {
            switch (State.stripMode)
            {
                case StripMode::ColdOnly:
                    State.pwmCold = clampRange(State.brightness, State.coldMin, State.coldMax);
                    State.pwmWarm = 0;
                    break;
                case StripMode::WarmOnly:
                    State.pwmCold = 0;
                    State.pwmWarm = clampRange(State.brightness, State.warmMin, State.warmMax);
                    break;
                case StripMode::DualWhite:
                default:
                    State.pwmCold = mixChannel(State.brightness, State.kelvin, true, State.coldMin, State.coldMax);
                    State.pwmWarm = mixChannel(State.brightness, State.kelvin, false, State.warmMin, State.warmMax);
                    break;
            }
        }

        analogWrite(PeripheralConfig::PWM_COLD_PIN, State.pwmCold);
        analogWrite(PeripheralConfig::PWM_WARM_PIN, State.pwmWarm);
    }
}
