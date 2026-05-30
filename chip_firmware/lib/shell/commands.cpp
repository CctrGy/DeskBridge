#include "commands.h"

#include <Arduino.h>
#include <RTClib.h>
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bus/DeskBus.h"
#include "config/DeskConfig.h"
#include "core/DeskBridgeVersion.h"
#include "core/DeskSettings.h"
#include "core/McuMonitor.h"
#include "libs.h"
#include "usb/DeskUSB.h"

#ifndef DESKBRIDGE_USB_VID
    #define DESKBRIDGE_USB_VID 0x1209
#endif

#ifndef DESKBRIDGE_USB_PID
    #define DESKBRIDGE_USB_PID 0xDB01
#endif

namespace
{
    constexpr uint8_t maxTokens = 5;
    constexpr uint8_t maxTokenLength = 24;

    struct Tokens
    {
        char value[maxTokens][maxTokenLength + 1] = {};
        uint8_t count = 0;
    };

    bool equals(const char *left, const char *right)
    {
        return strcmp(left, right) == 0;
    }

    bool equalsIgnoreCase(const char *left, const char *right)
    {
        while (*left != '\0' && *right != '\0')
        {
            if (tolower(static_cast<unsigned char>(*left)) != tolower(static_cast<unsigned char>(*right)))
            {
                return false;
            }
            ++left;
            ++right;
        }

        return *left == '\0' && *right == '\0';
    }

    void reply(Shell &shell, const char *text)
    {
        shell.writeLine(text);
    }

    void replyFormat(Shell &shell, const char *format, ...)
    {
        char buffer[160] = {};
        va_list args;
        va_start(args, format);
        vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        shell.writeLine(buffer);
    }

    void replyHeader(Shell &shell, const char *title)
    {
        reply(shell, "");
        replyFormat(shell, "[%s]", title);
    }

    Tokens tokenize(const char *line)
    {
        Tokens tokens;
        uint8_t tokenLength = 0;

        for (const char *cursor = line; *cursor != '\0'; ++cursor)
        {
            if (isspace(static_cast<unsigned char>(*cursor)))
            {
                if (tokenLength > 0)
                {
                    tokens.value[tokens.count][tokenLength] = '\0';
                    ++tokens.count;
                    tokenLength = 0;
                    if (tokens.count >= maxTokens)
                    {
                        break;
                    }
                }
                continue;
            }

            if (tokens.count < maxTokens && tokenLength < maxTokenLength)
            {
                tokens.value[tokens.count][tokenLength++] = *cursor;
            }
        }

        if (tokenLength > 0 && tokens.count < maxTokens)
        {
            tokens.value[tokens.count][tokenLength] = '\0';
            ++tokens.count;
        }

        return tokens;
    }

    uint32_t clampU32(uint32_t value, uint32_t minValue, uint32_t maxValue)
    {
        if (value < minValue)
        {
            return minValue;
        }
        if (value > maxValue)
        {
            return maxValue;
        }
        return value;
    }

    bool parseU32(const char *text, uint32_t &value)
    {
        char *end = nullptr;
        const unsigned long parsed = strtoul(text, &end, 10);
        if (end == text || *end != '\0')
        {
            return false;
        }
        value = static_cast<uint32_t>(parsed);
        return true;
    }

    bool parseDurationMs(const char *text, uint32_t &value)
    {
        char *end = nullptr;
        const float parsed = strtof(text, &end);
        if (end == text)
        {
            return false;
        }

        if (equals(end, "s"))
        {
            value = static_cast<uint32_t>(parsed * 1000.0f);
            return true;
        }

        if (equals(end, "ms") || *end == '\0')
        {
            value = static_cast<uint32_t>(parsed);
            return true;
        }

        return false;
    }

    bool parseRtcValue(const char *text, DateTime &dateTime)
    {
        int year = 0;
        int month = 0;
        int day = 0;
        int hour = 0;
        int minute = 0;
        int second = 0;

        if (sscanf(text, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6 &&
            sscanf(text, "%d-%d-%d_%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6)
        {
            return false;
        }

        if (year < 2000 || month < 1 || month > 12 || day < 1 || day > 31 ||
            hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59)
        {
            return false;
        }

        dateTime = DateTime(year, month, day, hour, minute, second);
        return true;
    }

    DeskBus::Device sensorBySelector(const char *selector, bool &ok)
    {
        uint32_t index = 0;
        if (parseU32(selector, index))
        {
            switch (index)
            {
                case 0: ok = true; return DeskBus::Device::ENS160;
                case 1: ok = true; return DeskBus::Device::AHT21;
                case 2: ok = true; return DeskBus::Device::VEML7700;
                case 3: ok = true; return DeskBus::Device::SCD41;
                case 4: ok = true; return DeskBus::Device::SHT21;
                default: ok = false; return DeskBus::Device::ENS160;
            }
        }

        for (uint8_t index = 0; index < static_cast<uint8_t>(DeskBus::Device::Count); ++index)
        {
            const DeskBus::Device device = static_cast<DeskBus::Device>(index);
            if (equalsIgnoreCase(selector, DeskBus::name(device)))
            {
                ok = true;
                return device;
            }
        }

        ok = false;
        return DeskBus::Device::ENS160;
    }

    void printDeviceLine(Shell &shell, DeskBus::Device device)
    {
        replyFormat(shell, "  %-8s addr=%-5s state=%s", DeskBus::name(device), DeskBus::addressText(device), DeskBus::ready(device) ? "ok" : "fail");
    }

    void printSensorValue(Shell &shell, DeskBus::Device device)
    {
        const DeskBus::Measurements &data = DeskBus::measurements();
        printDeviceLine(shell, device);

        switch (device)
        {
            case DeskBus::Device::ENS160:
                replyFormat(shell, "  aqi      : %u", data.airQualityIndex);
                replyFormat(shell, "  eco2     : %d ppm", data.ensEco2Ppm);
                replyFormat(shell, "  tvoc     : %d ppb", data.tvocPpb);
                break;
            case DeskBus::Device::AHT21:
                replyFormat(shell, "  temp     : %.1f C", data.temperature);
                replyFormat(shell, "  humidity : %.1f %%", data.humidity);
                break;
            case DeskBus::Device::VEML7700:
                replyFormat(shell, "  lux      : %.1f lx", data.lux);
                break;
            case DeskBus::Device::SCD41:
                replyFormat(shell, "  co2      : %d ppm", data.co2Ppm);
                break;
            case DeskBus::Device::SHT21:
                replyFormat(shell, "  temp     : %.1f C", data.shtTemperature);
                replyFormat(shell, "  humidity : %.1f %%", data.shtHumidity);
                break;
            default:
                break;
        }
    }

    void handleInfo(Shell &shell)
    {
        replyHeader(shell, "info");
        replyFormat(shell, "  device   : %s", DeskBridgeVersion::DEVICE_NAME);
        replyFormat(shell, "  firmware : %s", DeskBridgeVersion::FIRMWARE);
        replyFormat(shell, "  protocol : %u", DeskBridgeVersion::SERIAL_PROTOCOL);
        replyFormat(shell, "  usb      : %s", DeskUSB::mounted() ? "mounted" : "unmounted");
        replyFormat(shell, "  cdc      : %s", DeskUSB::cdcConnected(DeskUSB::CONTROL) ? "open" : "closed");
        replyFormat(shell, "  uptime   : %lus", static_cast<unsigned long>(McuMonitor::uptimeSeconds()));
    }

    void handleUsb(Shell &shell)
    {
        replyHeader(shell, "usb");
        replyFormat(shell, "  vid      : 0x%04X", DESKBRIDGE_USB_VID);
        replyFormat(shell, "  pid      : 0x%04X", DESKBRIDGE_USB_PID);
        replyFormat(shell, "  bcd      : 0x%04X", DeskBridgeVersion::USB_DEVICE_BCD);
        replyFormat(shell, "  mounted  : %s", DeskUSB::mounted() ? "yes" : "no");
        replyFormat(shell, "  vbus     : %s", DeskUSB::vbusPresent() ? "yes" : "no");
        replyFormat(shell, "  cdc      : %s", DeskUSB::cdcConnected(DeskUSB::CONTROL) ? "open" : "closed");
    }

    void handleProgram(Shell &shell)
    {
        const DeskBridgeVersion::Info version = DeskBridgeVersion::current();
        replyHeader(shell, "program");
        replyFormat(shell, "  name     : %s", version.deviceName);
        replyFormat(shell, "  firmware : %s", version.firmware);
        replyFormat(shell, "  semver   : %u.%u.%u", version.major, version.minor, version.patch);
        replyFormat(shell, "  protocol : %u", version.serialProtocol);
        replyFormat(shell, "  build    : %lu", static_cast<unsigned long>(version.buildUnixTime));
    }

    void handleSys(Shell &shell, const Tokens &tokens)
    {
        if (tokens.count == 1 || equals(tokens.value[1], "local"))
        {
            replyHeader(shell, "system");
            replyFormat(shell, "  chip     : %s", McuMonitor::chipName());
            replyFormat(shell, "  clock    : %lu Hz", static_cast<unsigned long>(McuMonitor::coreClockHz()));
            replyFormat(shell, "  temp     : %.1f C", McuMonitor::temperatureC());
            replyFormat(shell, "  vdda     : %.2f V", McuMonitor::vddaVolts());
            replyFormat(shell, "  reset    : %s", McuMonitor::resetReason());
            replyFormat(shell, "  uptime   : %lus", static_cast<unsigned long>(McuMonitor::uptimeSeconds()));
            return;
        }

        if (equals(tokens.value[1], "periferics") || equals(tokens.value[1], "peripherals"))
        {
            replyHeader(shell, "system peripherals");
            printDeviceLine(shell, DeskBus::Device::DS3231);
            printDeviceLine(shell, DeskBus::Device::AT24C32);
            return;
        }

        reply(shell, "ERR use: sys? | sys local | sys periferics");
    }

    void handleBusQuery(Shell &shell)
    {
        const DeskSettings::Config &settings = DeskSettings::dataConst();
        replyHeader(shell, "bus");
        replyFormat(shell, "  time_read: %lums", static_cast<unsigned long>(settings.sensorSampleIntervalMs));
        replyFormat(shell, "  samples  : %u", settings.sensorSampleCount);
        reply(shell, "  sensors:");
        printDeviceLine(shell, DeskBus::Device::ENS160);
        printDeviceLine(shell, DeskBus::Device::AHT21);
        printDeviceLine(shell, DeskBus::Device::VEML7700);
        printDeviceLine(shell, DeskBus::Device::SCD41);
        printDeviceLine(shell, DeskBus::Device::SHT21);
    }

    void handleBus(Shell &shell, const Tokens &tokens)
    {
        if (tokens.count < 2)
        {
            reply(shell, "ERR use: bus scan|time_read|samples|sensor [value]");
            return;
        }

        if (equals(tokens.value[1], "scan"))
        {
            uint8_t addresses[12] = {};
            const uint8_t count = DeskBus::scanSensors(addresses, sizeof(addresses));
            replyHeader(shell, "bus scan");
            replyFormat(shell, "  found    : %u", count);
            for (uint8_t index = 0; index < count; ++index)
            {
                replyFormat(shell, "  addr[%u]  : 0x%02X", index, addresses[index]);
            }
            return;
        }

        if (equals(tokens.value[1], "sensor"))
        {
            if (tokens.count < 3)
            {
                handleSensorQuery(shell);
                return;
            }

            bool ok = false;
            const DeskBus::Device device = sensorBySelector(tokens.value[2], ok);
            if (!ok)
            {
                reply(shell, "ERR unknown sensor");
                return;
            }

            replyHeader(shell, "bus sensor");
            printSensorValue(shell, device);
            return;
        }

        if (equals(tokens.value[1], "time_read"))
        {
            if (tokens.count == 2)
            {
                replyHeader(shell, "bus time_read");
                replyFormat(shell, "  value    : %lums", static_cast<unsigned long>(DeskSettings::dataConst().sensorSampleIntervalMs));
                return;
            }

            uint32_t value = 0;
            if (!parseDurationMs(tokens.value[2], value))
            {
                reply(shell, "ERR invalid time_read value");
                return;
            }

            DeskSettings::data().sensorSampleIntervalMs = clampU32(value, SENSOR_SAMPLE_INTERVAL_MS_MIN_DEFAULT, SENSOR_SAMPLE_INTERVAL_MS_MAX_DEFAULT);
            DeskSettings::markDirty();
            replyFormat(shell, "OK bus.time_read = %lums", static_cast<unsigned long>(DeskSettings::dataConst().sensorSampleIntervalMs));
            return;
        }

        if (equals(tokens.value[1], "samples"))
        {
            if (tokens.count == 2)
            {
                replyHeader(shell, "bus samples");
                replyFormat(shell, "  value    : %u", DeskSettings::dataConst().sensorSampleCount);
                return;
            }

            uint32_t value = 0;
            if (!parseU32(tokens.value[2], value))
            {
                reply(shell, "ERR invalid samples value");
                return;
            }

            DeskSettings::data().sensorSampleCount = static_cast<uint8_t>(clampU32(value, SENSOR_SAMPLE_COUNT_MIN_DEFAULT, SENSOR_SAMPLE_COUNT_MAX_DEFAULT));
            DeskSettings::markDirty();
            replyFormat(shell, "OK bus.samples = %u", DeskSettings::dataConst().sensorSampleCount);
            return;
        }

        reply(shell, "ERR use: bus scan|time_read|samples|sensor [value]");
    }

    void handleSensorQuery(Shell &shell)
    {
        replyHeader(shell, "bus sensors");
        reply(shell, "  0 : ENS160");
        reply(shell, "  1 : AHT21");
        reply(shell, "  2 : VEML7700");
        reply(shell, "  3 : SCD41");
        reply(shell, "  4 : SHT21");
    }

    void handleRtcQuery(Shell &shell)
    {
        replyHeader(shell, "rtc");
        replyFormat(shell, "  state    : %s", DeskBus::ready(DeskBus::Device::DS3231) ? "ok" : "fail");
        replyFormat(shell, "  lost_pwr : %s", DeskBus::rtcLostPower() ? "yes" : "no");
    }

    void handleRtc(Shell &shell, const Tokens &tokens)
    {
        if (tokens.count < 2)
        {
            reply(shell, "ERR use: rtc get|set [YYYY-MM-DDTHH:MM:SS]");
            return;
        }

        if (equals(tokens.value[1], "get"))
        {
            const DateTime now = DeskBus::now();
            replyHeader(shell, "rtc time");
            replyFormat(shell, "  value    : %04u-%02u-%02uT%02u:%02u:%02u", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
            return;
        }

        if (equals(tokens.value[1], "set"))
        {
            if (tokens.count < 3)
            {
                reply(shell, "ERR missing rtc value");
                return;
            }

            DateTime value;
            if (!parseRtcValue(tokens.value[2], value))
            {
                reply(shell, "ERR invalid rtc value");
                return;
            }

            reply(shell, DeskBus::setRtc(value) ? "OK rtc.set" : "ERR rtc not ready");
            return;
        }

        reply(shell, "ERR use: rtc get|set [YYYY-MM-DDTHH:MM:SS]");
    }
}

namespace DeskShellCommands
{
    bool handle(const char *line, Shell &shell)
    {
        const Tokens tokens = tokenize(line);
        if (tokens.count == 0)
        {
            return true;
        }

        if (equals(tokens.value[0], "ping"))
        {
            reply(shell, "pong");
            return true;
        }

        if (equals(tokens.value[0], "info?"))
        {
            handleInfo(shell);
            return true;
        }

        if (equals(tokens.value[0], "usb?"))
        {
            handleUsb(shell);
            return true;
        }

        if (equals(tokens.value[0], "program?"))
        {
            handleProgram(shell);
            return true;
        }

        if (equals(tokens.value[0], "sys?") || equals(tokens.value[0], "sys"))
        {
            handleSys(shell, tokens);
            return true;
        }

        if (equals(tokens.value[0], "bus?"))
        {
            handleBusQuery(shell);
            return true;
        }

        if (equals(tokens.value[0], "bus"))
        {
            handleBus(shell, tokens);
            return true;
        }

        if (equals(tokens.value[0], "rtc?"))
        {
            handleRtcQuery(shell);
            return true;
        }

        if (equals(tokens.value[0], "rtc"))
        {
            handleRtc(shell, tokens);
            return true;
        }

        return false;
    }
}
