#include "core/StatusIndicator.h"

#include <Arduino.h>

#include "config/DeskConfig.h"
#include "config/pins_config.h"
#include "usb/DeskUSB.h"

namespace
{
    uint32_t lastBlinkMs = 0;
    bool ledOn = false;
    bool ledEnabled = true;
}

namespace StatusIndicator
{
    void begin()
    {
        pinMode(STATUS_LED_PIN, OUTPUT);
        digitalWrite(STATUS_LED_PIN, HIGH);
    }

    void update()
    {
        if (!ledEnabled || !DeskUSB::cdcConnected(DeskUSB::CONTROL))
        {
            ledOn = false;
            digitalWrite(STATUS_LED_PIN, HIGH);
            return;
        }

        const uint32_t now = millis();
        if (now - lastBlinkMs < STATUS_LED_BLINK_INTERVAL_MS_DEFAULT)
        {
            return;
        }

        lastBlinkMs = now;
        ledOn = !ledOn;
        digitalWrite(STATUS_LED_PIN, ledOn ? LOW : HIGH);
    }

    bool enabled()
    {
        return ledEnabled;
    }

    void setEnabled(bool value)
    {
        ledEnabled = value;
        ledOn = false;
        digitalWrite(STATUS_LED_PIN, HIGH);
    }
}
