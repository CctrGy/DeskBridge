#include "core/PeripheralState.h"

#include <Arduino.h>
#include <string.h>

PeripheralState State;

namespace
{
    uint16_t clampPwm(uint16_t value)
    {
        return value > PeripheralConfig::PWM_MAX ? PeripheralConfig::PWM_MAX : value;
    }
}

void resetStateDefaults()
{
    State.stripEnabled = false;
    State.stripMode = StripMode::DualWhite;
    State.brightness = PeripheralConfig::DEFAULT_BRIGHTNESS;
    State.kelvin = PeripheralConfig::DEFAULT_KELVIN;
    State.coldMin = 0;
    State.coldMax = PeripheralConfig::PWM_MAX;
    State.warmMin = 0;
    State.warmMax = PeripheralConfig::PWM_MAX;
    State.brightnessStep = PeripheralConfig::DEFAULT_STEP;
    State.kelvinStep = PeripheralConfig::DEFAULT_STEP;
    State.debounceMs = PeripheralConfig::DEFAULT_DEBOUNCE_MS;
    State.clickMs = PeripheralConfig::DEFAULT_CLICK_MS;
    State.longMs = PeripheralConfig::DEFAULT_LONG_MS;
    State.repeatMs = PeripheralConfig::DEFAULT_REPEAT_MS;
    State.brightnessUp = true;
    State.kelvinUp = true;
    clearButtonEdges();
    State.supplyMv = PeripheralConfig::DEFAULT_SUPPLY_MV;
    normalizeState();
}

void normalizeState()
{
    State.coldMin = clampPwm(State.coldMin);
    State.coldMax = clampPwm(State.coldMax);
    if (State.coldMax < State.coldMin)
    {
        State.coldMax = State.coldMin;
    }

    State.warmMin = clampPwm(State.warmMin);
    State.warmMax = clampPwm(State.warmMax);
    if (State.warmMax < State.warmMin)
    {
        State.warmMax = State.warmMin;
    }

    State.brightness = clampPwm(State.brightness);
    State.kelvin = clampPwm(State.kelvin);
    State.brightnessStep = constrain(State.brightnessStep, 1, PeripheralConfig::PWM_MAX);
    State.kelvinStep = constrain(State.kelvinStep, 1, PeripheralConfig::PWM_MAX);
    State.debounceMs = constrain(State.debounceMs, 1, 250);
    State.clickMs = constrain(State.clickMs, 50, 3000);
    State.longMs = constrain(State.longMs, 50, 5000);
    State.repeatMs = constrain(State.repeatMs, 10, 2000);
    State.supplyMv = constrain(State.supplyMv, 0, 60000);
}

void markEvent(uint16_t flags)
{
    noInterrupts();
    State.eventFlags |= flags;
    interrupts();
}

void clearEvents()
{
    noInterrupts();
    State.eventFlags = 0;
    interrupts();
}

uint16_t readEvents()
{
    noInterrupts();
    const uint16_t events = State.eventFlags;
    interrupts();
    return events;
}

void syncEventOutput()
{
    digitalWrite(PeripheralConfig::EVENT_OUT_PIN, readEvents() != 0 ? HIGH : LOW);
}

void syncStateOutput()
{
    bool anyButtonDown = false;
    for (uint8_t index = 0; index < PeripheralConfig::BUTTON_COUNT; ++index)
    {
        anyButtonDown |= State.buttonDown[index];
    }

    digitalWrite(PeripheralConfig::STATE_OUT_PIN, anyButtonDown ? HIGH : LOW);
}

void markButtonEdge(uint8_t buttonIndex, ButtonEdge edge)
{
    if (buttonIndex >= PeripheralConfig::BUTTON_COUNT || edge == ButtonEdge::None)
    {
        return;
    }

    const uint8_t mask = 1 << buttonIndex;
    State.lastButtonEventIndex = buttonIndex;
    State.lastButtonEventEdge = edge;

    if (edge == ButtonEdge::Pressed)
    {
        State.pendingButtonPressedMask |= mask;
        State.buttonDown[buttonIndex] = true;
    }
    else
    {
        State.pendingButtonReleasedMask |= mask;
        State.buttonDown[buttonIndex] = false;
    }

    markEvent(EVENT_BUTTON_EDGE);
}

void clearButtonEdges()
{
    State.pendingButtonPressedMask = 0;
    State.pendingButtonReleasedMask = 0;
    State.lastButtonEventIndex = 0xFF;
    State.lastButtonEventEdge = ButtonEdge::None;
    noInterrupts();
    State.eventFlags &= ~EVENT_BUTTON_EDGE;
    interrupts();
}

void setCoreAction(const char *hexCode)
{
    if (hexCode == nullptr)
    {
        return;
    }

    strncpy(State.pendingCoreActionHex, hexCode, sizeof(State.pendingCoreActionHex) - 1);
    State.pendingCoreActionHex[sizeof(State.pendingCoreActionHex) - 1] = '\0';
    State.coreActionPending = true;
}

bool consumeCoreAction(char *buffer, uint8_t length)
{
    if (buffer == nullptr || length == 0 || !State.coreActionPending)
    {
        return false;
    }

    strncpy(buffer, State.pendingCoreActionHex, length - 1);
    buffer[length - 1] = '\0';
    State.pendingCoreActionHex[0] = '\0';
    State.coreActionPending = false;
    return true;
}
