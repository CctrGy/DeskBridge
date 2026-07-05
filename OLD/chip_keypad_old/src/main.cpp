#include <Arduino.h>

#include "bus/UARTProtocol.h"
#include "core/PeripheralState.h"
#include "input/ButtonControls.h"
#include "light/StripPwm.h"
#include "sensors/PowerMonitor.h"

void setup()
{
    pinMode(PeripheralConfig::EVENT_OUT_PIN, OUTPUT);
    digitalWrite(PeripheralConfig::EVENT_OUT_PIN, LOW);
    pinMode(PeripheralConfig::STATE_OUT_PIN, OUTPUT);
    digitalWrite(PeripheralConfig::STATE_OUT_PIN, LOW);

    resetStateDefaults();
    StripPwm::begin();
    ButtonControls::begin();
    PowerMonitor::begin();
    UARTProtocol::begin();
}

void loop()
{
    ButtonControls::update();
    PowerMonitor::update();
    StripPwm::update();
    UARTProtocol::update();
    syncEventOutput();
    syncStateOutput();
    delay(1);
}
