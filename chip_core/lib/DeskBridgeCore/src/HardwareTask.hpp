#pragma once

#include <Arduino.h>
#include <stdint.h>

#include "Crono.hpp"
#include "config/DeskConfig.h"

class HardwareTask
{
public:
    void begin();
    void updateBackend();
    void updateInterface();
    void update();

    uint32_t cycles() const
    {
        return cycles_;
    }

    uint32_t lastLoopMicros() const
    {
        return lastLoopMicros_;
    }

    uint32_t maxLoopMicros() const
    {
        return maxLoopMicros_;
    }

private:
    void logFirmwareIdentity();
    void logHeartbeat();
    void updateUsbNotifications();

    uint32_t cycles_ = 0;
    uint32_t lastLoopMicros_ = 0;
    uint32_t maxLoopMicros_ = 0;
    uint32_t loopStartUs_ = 0;
    bool identityLogged_ = false;
    bool keypadPresenceKnown_ = false;
    bool lastKeypadPresent_ = false;
    Crono heartbeat_{HARDWARE_TASK_HEARTBEAT_MS_DEFAULT};
};
