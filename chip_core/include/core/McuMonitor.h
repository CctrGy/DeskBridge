#pragma once

#include <stdint.h>

namespace McuMonitor
{
    void begin();
    void update();

    bool ready();
    float temperatureC();
    float vddaVolts();
    uint32_t coreClockHz();
    uint32_t uptimeSeconds();
    const char *chipName();
    const char *uniqueIdHex();
    const char *resetReason();
}
