#include "core/DeskBridgeVersion.h"

#ifndef BUILD_UNIX_TIME
#define BUILD_UNIX_TIME 0
#endif

namespace DeskBridgeVersion
{
    Info current()
    {
        return {
            DEVICE_NAME,
            FIRMWARE,
            MAJOR,
            MINOR,
            PATCH,
            SERIAL_PROTOCOL,
            USB_DEVICE_BCD,
            static_cast<uint32_t>(BUILD_UNIX_TIME),
        };
    }
}
