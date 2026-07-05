#pragma once

#include <stdint.h>

namespace DeskBridgeVersion
{
    // Firmware semantic version. Update these three values for firmware releases.
    inline constexpr uint8_t MAJOR = 0;
    inline constexpr uint8_t MINOR = 4;
    inline constexpr uint8_t PATCH = 0;

    inline constexpr char FIRMWARE[] = "0.4.0";
    inline constexpr char DEVICE_NAME[] = "DeskBridge chip_core";

    // Protocol version used by the PC app wire framing.
    inline constexpr uint8_t SERIAL_PROTOCOL = 0x04;

    // USB bcdDevice revision shown by the host as the device release number.
    // It is separate from semantic versioning because USB stores BCD digits.
    inline constexpr uint16_t USB_DEVICE_BCD = 0x0400;

    struct Info
    {
        const char *deviceName;
        const char *firmware;
        uint8_t major;
        uint8_t minor;
        uint8_t patch;
        uint8_t serialProtocol;
        uint16_t usbDeviceBcd;
        uint32_t buildUnixTime;
    };

    Info current();
}
