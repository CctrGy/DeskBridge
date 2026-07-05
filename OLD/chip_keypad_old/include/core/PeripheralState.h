#pragma once

#include <stdint.h>

#include "config/PeripheralConfig.h"

enum class StripMode : uint8_t
{
    ColdOnly = 0,
    WarmOnly = 1,
    DualWhite = 2,
};

enum EventFlag : uint16_t
{
    EVENT_POWER_CLICK = 1 << 0,
    EVENT_ADJUST_CLICK = 1 << 1,
    EVENT_POWER_LONG = 1 << 2,
    EVENT_ADJUST_LONG = 1 << 3,
    EVENT_BRIGHTNESS_CHANGED = 1 << 4,
    EVENT_KELVIN_CHANGED = 1 << 5,
    EVENT_CORE_COMMAND = 1 << 6,
    EVENT_POWER_RELEASE = 1 << 7,
    EVENT_ADJUST_RELEASE = 1 << 8,
    EVENT_BUTTON_EDGE = 1 << 9,
};

enum class ButtonEdge : uint8_t
{
    None = 0,
    Pressed = 1,
    Released = 2,
};

struct PeripheralState
{
    volatile uint16_t eventFlags = 0;

    bool stripEnabled = false;
    StripMode stripMode = StripMode::DualWhite;
    uint16_t brightness = PeripheralConfig::DEFAULT_BRIGHTNESS;
    uint16_t kelvin = PeripheralConfig::DEFAULT_KELVIN;
    uint16_t coldMin = 0;
    uint16_t coldMax = PeripheralConfig::PWM_MAX;
    uint16_t warmMin = 0;
    uint16_t warmMax = PeripheralConfig::PWM_MAX;
    uint16_t brightnessStep = PeripheralConfig::DEFAULT_STEP;
    uint16_t kelvinStep = PeripheralConfig::DEFAULT_STEP;

    uint16_t debounceMs = PeripheralConfig::DEFAULT_DEBOUNCE_MS;
    uint16_t clickMs = PeripheralConfig::DEFAULT_CLICK_MS;
    uint16_t longMs = PeripheralConfig::DEFAULT_LONG_MS;
    uint16_t repeatMs = PeripheralConfig::DEFAULT_REPEAT_MS;

    uint16_t pwmCold = 0;
    uint16_t pwmWarm = 0;
    bool powerButtonDown = false;
    bool adjustButtonDown = false;
    bool buttonDown[PeripheralConfig::BUTTON_COUNT] = {};
    uint8_t pendingButtonPressedMask = 0;
    uint8_t pendingButtonReleasedMask = 0;
    uint8_t lastButtonEventIndex = 0xFF;
    ButtonEdge lastButtonEventEdge = ButtonEdge::None;
    bool brightnessUp = true;
    bool kelvinUp = true;

    int16_t currentMa = 0;
    uint16_t currentRawAdc = 0;
    uint16_t voltageRawAdc = 0;
    uint16_t supplyMv = PeripheralConfig::DEFAULT_SUPPLY_MV;
    char pendingCoreActionHex[9] = {};
    bool coreActionPending = false;
};

extern PeripheralState State;

void resetStateDefaults();
void normalizeState();
void markEvent(uint16_t flags);
void clearEvents();
uint16_t readEvents();
void syncEventOutput();
void syncStateOutput();
void markButtonEdge(uint8_t buttonIndex, ButtonEdge edge);
void clearButtonEdges();
void setCoreAction(const char *hexCode);
bool consumeCoreAction(char *buffer, uint8_t length);
