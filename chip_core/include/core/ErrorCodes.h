#pragma once

#include <stdint.h>

namespace DeskError
{
    inline constexpr uint32_t ShellLineTooLong = 0x00010001;
    inline constexpr uint32_t ShellUnknownCommand = 0x00010002;
    inline constexpr uint32_t CommandUsage = 0x00011001;
    inline constexpr uint32_t InvalidArgument = 0x00011002;
    inline constexpr uint32_t BusUnknownSensor = 0x00020001;
    inline constexpr uint32_t RtcMissingValue = 0x00030001;
    inline constexpr uint32_t RtcInvalidValue = 0x00030002;
    inline constexpr uint32_t RtcNotReady = 0x00030003;
    inline constexpr uint32_t KeypadMissing = 0x00040001;
    inline constexpr uint32_t LightUnknownField = 0x00041001;
    inline constexpr uint32_t ResetUnknownTarget = 0x00050001;
    inline constexpr uint32_t GenericCommandError = 0x0001FFFF;
}

namespace DeskMarker
{
    inline constexpr uint32_t ShellBegin = 0x0000A001;
    inline constexpr uint32_t LightConfigUpdated = 0x0000A101;
    inline constexpr uint32_t KeypadActionUpdated = 0x0000A102;
    inline constexpr uint32_t ResetKeypad = 0x0000A201;
    inline constexpr uint32_t ResetSettings = 0x0000A202;
    inline constexpr uint32_t ResetCore = 0x0000A203;
}
