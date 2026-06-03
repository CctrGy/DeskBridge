#pragma once

#include <stdint.h>

namespace KeypadPeripheral
{
    enum EventFlag : uint16_t
    {
        EventPowerClick = 1 << 0,
        EventAdjustClick = 1 << 1,
        EventPowerLong = 1 << 2,
        EventAdjustLong = 1 << 3,
        EventBrightnessChanged = 1 << 4,
        EventKelvinChanged = 1 << 5,
        EventCoreCommand = 1 << 6,
        EventPowerRelease = 1 << 7,
        EventAdjustRelease = 1 << 8,
        EventButtonEdge = 1 << 9,
    };

    struct Snapshot
    {
        bool present = false;
        uint8_t protocol = 0;
        char protocolText[8] = {};
        uint8_t status = 0;
        uint16_t events = 0;
        bool eventInterruptEnabled = false;
        uint32_t eventInterruptCount = 0;
        bool statePin = false;
        uint8_t buttonPressedMask = 0;
        uint8_t buttonReleasedMask = 0;
        uint8_t buttonDownMask = 0;
        uint8_t lastButton = 0xFF;
        uint8_t lastButtonEdge = 0;
        uint8_t buttonAssignments[5] = {};
        char lastAction[9] = {};
        bool stripEnabled = false;
        uint8_t stripMode = 0;
        uint16_t brightness = 0;
        uint16_t kelvin = 0;
        uint16_t coldMin = 0;
        uint16_t coldMax = 0;
        uint16_t warmMin = 0;
        uint16_t warmMax = 0;
        uint16_t brightnessStep = 0;
        uint16_t kelvinStep = 0;
        uint16_t clickMs = 0;
        uint16_t longMs = 0;
        uint16_t repeatMs = 0;
        uint16_t pwmCold = 0;
        uint16_t pwmWarm = 0;
        int16_t currentMa = 0;
        uint16_t currentRaw = 0;
        uint16_t voltageRaw = 0;
        uint16_t supplyMv = 0;
        uint32_t powerMw = 0;
    };

    void prepareResetHold();
    void begin();
    void update();
    void serviceEvents();
    bool present();
    bool eventPending();
    bool stateActive();
    bool consumeInterruptFlag();
    bool consumeButtonEventNotification();
    void hardwareReset();
    void clearEvents();
    bool command(uint8_t command);
    bool writeU16(uint8_t regLow, uint16_t value);
    bool setButtonAction(uint8_t buttonIndex, uint8_t actionId);
    bool syncStripConfig();
    const Snapshot &snapshot();
}
