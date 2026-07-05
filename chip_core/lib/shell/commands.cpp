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
#include "bus/KeypadPeripheral.h"
#include "config/DeskConfig.h"
#include "core/DeskBridgeVersion.h"
#include "core/ErrorCodes.h"
#include "core/DeskSettings.h"
#include "core/McuMonitor.h"
#include "libs.h"
#include "light/StripLight.h"
#include "usb/DeskUSB.h"
#include "usb/usb_descriptors.h"
#include <Antenna.hpp>

namespace
{
    constexpr uint8_t maxTokens = 5;
    constexpr uint8_t maxTokenLength = 64;

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
        char buffer[192] = {};
        if (strncmp(text, "ERR", 3) == 0)
        {
            const char *detail = text + 3;
            while (*detail == ' ')
            {
                ++detail;
            }

            uint32_t code = DeskError::GenericCommandError;
            if (strncmp(detail, "use:", 4) == 0)
            {
                code = DeskError::CommandUsage;
            }
            else if (strstr(detail, "invalid") != nullptr)
            {
                code = DeskError::InvalidArgument;
            }
            else if (strstr(detail, "unknown sensor") != nullptr)
            {
                code = DeskError::BusUnknownSensor;
            }
            else if (strstr(detail, "missing rtc") != nullptr)
            {
                code = DeskError::RtcMissingValue;
            }
            else if (strstr(detail, "rtc not ready") != nullptr)
            {
                code = DeskError::RtcNotReady;
            }
            else if (strstr(detail, "keypad missing") != nullptr)
            {
                code = DeskError::KeypadMissing;
            }
            else if (strstr(detail, "unknown light field") != nullptr)
            {
                code = DeskError::LightUnknownField;
            }

            snprintf(buffer, sizeof(buffer), "[ERR] 0x%08lX %s", static_cast<unsigned long>(code), detail);
        }
        else
        {
            snprintf(buffer, sizeof(buffer), "[RSP] %s", text);
        }
        shell.writeLine(buffer);
    }

    void replyList(Shell &shell, const char *name, const char *format, ...)
    {
        char values[160] = {};
        char buffer[192] = {};
        va_list args;
        va_start(args, format);
        vsnprintf(values, sizeof(values), format, args);
        va_end(args);
        snprintf(buffer, sizeof(buffer), "[RSP] %s: %s", name, values);
        shell.writeLine(buffer);
    }

    void handleSensorQuery(Shell &shell);

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

    uint16_t clampU16(uint32_t value, uint16_t minValue, uint16_t maxValue)
    {
        return static_cast<uint16_t>(clampU32(value, minValue, maxValue));
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

    bool parseKeypadActionId(const char *text, uint8_t &value)
    {
        uint32_t numeric = 0;
        if (parseU32(text, numeric) && numeric <= 9)
        {
            value = static_cast<uint8_t>(numeric);
            return true;
        }

        if (equalsIgnoreCase(text, "none")) { value = 0; return true; }
        if (equalsIgnoreCase(text, "mute") || equalsIgnoreCase(text, "media_mute")) { value = 1; return true; }
        if (equalsIgnoreCase(text, "vol_up") || equalsIgnoreCase(text, "volume_up")) { value = 2; return true; }
        if (equalsIgnoreCase(text, "vol_down") || equalsIgnoreCase(text, "volume_down")) { value = 3; return true; }
        if (equalsIgnoreCase(text, "play") || equalsIgnoreCase(text, "play_pause")) { value = 4; return true; }
        if (equalsIgnoreCase(text, "next") || equalsIgnoreCase(text, "media_next")) { value = 5; return true; }
        if (equalsIgnoreCase(text, "prev") || equalsIgnoreCase(text, "previous") || equalsIgnoreCase(text, "media_previous")) { value = 6; return true; }
        if (equalsIgnoreCase(text, "f13")) { value = 7; return true; }
        if (equalsIgnoreCase(text, "f14")) { value = 8; return true; }
        if (equalsIgnoreCase(text, "f15")) { value = 9; return true; }

        return false;
    }

    const char *keypadActionName(uint8_t value)
    {
        switch (value)
        {
            case 1: return "mute";
            case 2: return "volume_up";
            case 3: return "volume_down";
            case 4: return "play_pause";
            case 5: return "next";
            case 6: return "previous";
            case 7: return "f13";
            case 8: return "f14";
            case 9: return "f15";
            default: return "none";
        }
    }

    bool parseBoolValue(const char *text, bool &value)
    {
        if (equalsIgnoreCase(text, "1") || equalsIgnoreCase(text, "on") || equalsIgnoreCase(text, "true") || equalsIgnoreCase(text, "yes"))
        {
            value = true;
            return true;
        }

        if (equalsIgnoreCase(text, "0") || equalsIgnoreCase(text, "off") || equalsIgnoreCase(text, "false") || equalsIgnoreCase(text, "no"))
        {
            value = false;
            return true;
        }

        return false;
    }

    bool parseStripMode(const char *text, StripLight::Mode &mode)
    {
        uint32_t value = 0;
        if (parseU32(text, value))
        {
            switch (value)
            {
                case 0: mode = StripLight::Mode::OneSimple; return true;
                case 1: mode = StripLight::Mode::TwoSimple; return true;
                case 2: mode = StripLight::Mode::Composite; return true;
                default: return false;
            }
        }

        if (equalsIgnoreCase(text, "one") || equalsIgnoreCase(text, "simple") || equalsIgnoreCase(text, "onesimple"))
        {
            mode = StripLight::Mode::OneSimple;
            return true;
        }
        if (equalsIgnoreCase(text, "two") || equalsIgnoreCase(text, "twosimple"))
        {
            mode = StripLight::Mode::TwoSimple;
            return true;
        }
        if (equalsIgnoreCase(text, "composite") || equalsIgnoreCase(text, "dual"))
        {
            mode = StripLight::Mode::Composite;
            return true;
        }

        return false;
    }

    void persistLightConfig()
    {
        DeskSettings::captureStripFromRuntime();
        DeskSettings::markDirty();
        KeypadPeripheral::syncStripConfig();
    }

    void marker(Shell &shell, uint32_t code, const char *text)
    {
        char buffer[96] = {};
        snprintf(buffer, sizeof(buffer), "[MRK] 0x%08lX %s", static_cast<unsigned long>(code), text);
        shell.writeLine(buffer);
    }

    void logLine(Shell &shell, const char *text)
    {
        char buffer[128] = {};
        snprintf(buffer, sizeof(buffer), "[LOG] %s", text);
        shell.writeLine(buffer);
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
        replyList(shell, "device", "name:%s, addr:%s, state:%s", DeskBus::name(device), DeskBus::addressText(device), DeskBus::ready(device) ? "ok" : "fail");
    }

    void printSensorValue(Shell &shell, DeskBus::Device device)
    {
        const DeskBus::Measurements &data = DeskBus::measurements();
        printDeviceLine(shell, device);

        switch (device)
        {
            case DeskBus::Device::ENS160:
                replyList(shell, "sensor_data", "name:%s, aqi:%u, eco2_ppm:%d, tvoc_ppb:%d", DeskBus::name(device), data.airQualityIndex, data.ensEco2Ppm, data.tvocPpb);
                break;
            case DeskBus::Device::AHT21:
                replyList(shell, "sensor_data", "name:%s, temp_c:%.1f, humidity_pct:%.1f", DeskBus::name(device), data.temperature, data.humidity);
                break;
            case DeskBus::Device::VEML7700:
                replyList(shell, "sensor_data", "name:%s, lux:%.1f", DeskBus::name(device), data.lux);
                break;
            case DeskBus::Device::SCD41:
                replyList(shell, "sensor_data", "name:%s, co2_ppm:%d", DeskBus::name(device), data.co2Ppm);
                break;
            case DeskBus::Device::SHT21:
                replyList(shell, "sensor_data", "name:%s, temp_c:%.1f, humidity_pct:%.1f", DeskBus::name(device), data.shtTemperature, data.shtHumidity);
                break;
            default:
                break;
        }
    }

    void handleInfo(Shell &shell)
    {
        replyList(shell, "info", "device:%s, product_id:%s, firmware:%s, protocol:0x%02X, usb:%s, cdc:%s, uptime_s:%lu",
                  DeskBridgeVersion::DEVICE_NAME,
                  McuMonitor::uniqueIdHex(),
                  DeskBridgeVersion::FIRMWARE,
                  DeskBridgeVersion::SERIAL_PROTOCOL,
                  DeskUSB::mounted() ? "mounted" : "unmounted",
                  DeskUSB::cdcConnected(DeskUSB::CONTROL) ? "open" : "closed",
                  static_cast<unsigned long>(McuMonitor::uptimeSeconds()));
    }

    void handleUsb(Shell &shell)
    {
        replyList(shell, "usb", "vid:0x%04X, pid:0x%04X, bcd:0x%04X, mounted:%s, vbus:%s, id:%s, cdc:%s",
                  DESKBRIDGE_USB_VID,
                  DESKBRIDGE_USB_PID,
                  DeskBridgeVersion::USB_DEVICE_BCD,
                  DeskUSB::mounted() ? "yes" : "no",
                  DeskUSB::vbusPresent() ? "yes" : "no",
                  DeskUSB::idGrounded() ? "grounded" : "floating",
                  DeskUSB::cdcConnected(DeskUSB::CONTROL) ? "open" : "closed");
    }

    void handleProgram(Shell &shell)
    {
        const DeskBridgeVersion::Info version = DeskBridgeVersion::current();
        replyList(shell, "program", "name:%s, firmware:%s, version:%u.%u.%u, protocol:0x%02X, usb_bcd:0x%04X, build:%lu",
                  version.deviceName,
                  version.firmware,
                  version.major,
                  version.minor,
                  version.patch,
                  version.serialProtocol,
                  version.usbDeviceBcd,
                  static_cast<unsigned long>(version.buildUnixTime));
    }

    void handleDisplayQuery(Shell &shell)
    {
        const DeskSettings::Config &settings = DeskSettings::dataConst();
        replyList(shell, "display", "design:%u, fps:%u, timeout_enabled:%s, timeout_ms:%lu, active_mode:%u, clock_start:%u, clock_end:%u",
                  settings.displayDesign,
                  settings.displayFrameRateFps,
                  settings.displayTimeoutEnabled ? "yes" : "no",
                  static_cast<unsigned long>(settings.displayTimeoutMs),
                  settings.displayActiveMode,
                  settings.displayActiveStartMinute,
                  settings.displayActiveEndMinute);
    }

    void handleDisplay(Shell &shell, const Tokens &tokens)
    {
        if (tokens.count < 2)
        {
            handleDisplayQuery(shell);
            return;
        }

        if (equals(tokens.value[1], "fps") || equals(tokens.value[1], "frame") || equals(tokens.value[1], "framerate"))
        {
            if (tokens.count < 3)
            {
                replyList(shell, "display_fps", "value:%u, min:%u, max:%u",
                          DeskSettings::dataConst().displayFrameRateFps,
                          DISPLAY_FRAME_RATE_FPS_MIN_DEFAULT,
                          DISPLAY_FRAME_RATE_FPS_MAX_DEFAULT);
                return;
            }

            uint32_t value = 0;
            if (!parseU32(tokens.value[2], value))
            {
                reply(shell, "ERR invalid display fps value");
                return;
            }

            DeskSettings::data().displayFrameRateFps = static_cast<uint8_t>(clampU32(value, DISPLAY_FRAME_RATE_FPS_MIN_DEFAULT, DISPLAY_FRAME_RATE_FPS_MAX_DEFAULT));
            DeskSettings::markDirty();
            replyList(shell, "ok", "display.fps:%u", DeskSettings::dataConst().displayFrameRateFps);
            return;
        }

        reply(shell, "ERR use: display? | display fps [15..50]");
    }

    void handleWirelessQuery(Shell &shell)
    {
        replyList(shell, "wireless", "enabled:%s, mode:%s, state:%s, pending:%s, ble_name:%s, ble_connected:%s, ble_count:%lu",
                  WirelessAntenna.enabled() ? "yes" : "no",
                  WirelessAntenna.modeText(),
                  WirelessAntenna.stateText(),
                  WirelessAntenna.pendingChanges() ? "yes" : "no",
                  WirelessAntenna.bleConfig().deviceName,
                  WirelessAntenna.bleConnected() ? "yes" : "no",
                  static_cast<unsigned long>(WirelessAntenna.bleConnectionCount()));
        if (WirelessAntenna.bleRxPending())
        {
            replyList(shell, "wireless_rx", "value:%s", WirelessAntenna.bleLastRx());
        }
    }

    void handleWireless(Shell &shell, const Tokens &tokens)
    {
        if (tokens.count < 2)
        {
            handleWirelessQuery(shell);
            return;
        }

        if (equals(tokens.value[1], "state"))
        {
            handleWirelessQuery(shell);
            return;
        }

        if (equals(tokens.value[1], "on") || equals(tokens.value[1], "ble") || equals(tokens.value[1], "pair"))
        {
            WirelessAntenna.setEnabled(true);
            WirelessAntenna.setMode(Antenna::Mode::BleDevice);
            const bool ok = WirelessAntenna.apply();
            replyList(shell, ok ? "ok" : "wireless_error", "ble:%s, state:%s, name:%s", ok ? "advertising" : "fail", WirelessAntenna.stateText(), WirelessAntenna.bleConfig().deviceName);
            return;
        }

        if (equals(tokens.value[1], "off") || equals(tokens.value[1], "stop"))
        {
            WirelessAntenna.stop();
            replyList(shell, "ok", "wireless:off");
            return;
        }

        if (equals(tokens.value[1], "enabled") || equals(tokens.value[1], "enable"))
        {
            if (tokens.count < 3)
            {
                replyList(shell, "wireless_enabled", "value:%s", WirelessAntenna.enabled() ? "yes" : "no");
                return;
            }

            bool value = false;
            if (!parseBoolValue(tokens.value[2], value))
            {
                reply(shell, "ERR invalid wireless enabled value");
                return;
            }

            WirelessAntenna.setEnabled(value);
            WirelessAntenna.setMode(value ? Antenna::Mode::BleDevice : Antenna::Mode::Off);
            const bool ok = WirelessAntenna.apply();
            replyList(shell, ok ? "ok" : "wireless_error", "enabled:%s, state:%s", WirelessAntenna.enabled() ? "yes" : "no", WirelessAntenna.stateText());
            return;
        }

        if (equals(tokens.value[1], "name"))
        {
            if (tokens.count < 3)
            {
                replyList(shell, "wireless_name", "value:%s", WirelessAntenna.bleConfig().deviceName);
                return;
            }

            Antenna::BleConfig config = WirelessAntenna.bleConfig();
            strncpy(config.deviceName, tokens.value[2], sizeof(config.deviceName) - 1);
            config.deviceName[sizeof(config.deviceName) - 1] = '\0';
            WirelessAntenna.setBleConfig(config);
            replyList(shell, "ok", "wireless.name:%s, pending:yes", WirelessAntenna.bleConfig().deviceName);
            return;
        }

        if (equals(tokens.value[1], "apply"))
        {
            const bool ok = WirelessAntenna.apply();
            replyList(shell, ok ? "ok" : "wireless_error", "apply:%s, state:%s", ok ? "done" : "fail", WirelessAntenna.stateText());
            return;
        }

        if (equals(tokens.value[1], "write"))
        {
            if (tokens.count < 3)
            {
                reply(shell, "ERR use: wireless write [text]");
                return;
            }

            const bool ok = WirelessAntenna.bleWrite(tokens.value[2]);
            replyList(shell, ok ? "ok" : "wireless_error", "write:%s", ok ? "done" : "fail");
            return;
        }

        if (equals(tokens.value[1], "rx"))
        {
            replyList(shell, "wireless_rx", "pending:%s, value:%s", WirelessAntenna.bleRxPending() ? "yes" : "no", WirelessAntenna.bleLastRx());
            WirelessAntenna.clearBleRx();
            return;
        }

        reply(shell, "ERR use: wireless? | wireless on|off|pair|state|enabled|name|apply|write|rx [value]");
    }

    void handleSys(Shell &shell, const Tokens &tokens)
    {
        if (tokens.count == 1 || equals(tokens.value[1], "local"))
        {
            replyList(shell, "system", "chip:%s, clock_hz:%lu, temp_c:%.1f, vdda_v:%.2f, reset:%s, uptime_s:%lu",
                      McuMonitor::chipName(),
                      static_cast<unsigned long>(McuMonitor::coreClockHz()),
                      McuMonitor::temperatureC(),
                      McuMonitor::vddaVolts(),
                      McuMonitor::resetReason(),
                      static_cast<unsigned long>(McuMonitor::uptimeSeconds()));
            return;
        }

        if (equals(tokens.value[1], "chip") || equals(tokens.value[1], "chips") ||
            equals(tokens.value[1], "periferics") || equals(tokens.value[1], "peripherals"))
        {
            replyList(shell, "system_peripherals", "count:%u", 3);
            printDeviceLine(shell, DeskBus::Device::DS3231);
            printDeviceLine(shell, DeskBus::Device::AT24C32);
            printDeviceLine(shell, DeskBus::Device::Keypad);
            return;
        }

        reply(shell, "ERR use: sys? | sys local | sys chips");
    }

    void handleBusQuery(Shell &shell)
    {
        const DeskSettings::Config &settings = DeskSettings::dataConst();
        replyList(shell, "bus", "time_read_ms:%lu, samples:%u, sensors:%u",
                  static_cast<unsigned long>(settings.sensorSampleIntervalMs),
                  settings.sensorSampleCount,
                  5);
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
            replyList(shell, "bus_scan", "found:%u", count);
            for (uint8_t index = 0; index < count; ++index)
            {
                replyList(shell, "bus_addr", "index:%u, addr:0x%02X", index, addresses[index]);
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

            printSensorValue(shell, device);
            return;
        }

        if (equals(tokens.value[1], "time_read"))
        {
            if (tokens.count == 2)
            {
                replyList(shell, "bus_time_read", "value_ms:%lu", static_cast<unsigned long>(DeskSettings::dataConst().sensorSampleIntervalMs));
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
            replyList(shell, "ok", "bus.time_read_ms:%lu", static_cast<unsigned long>(DeskSettings::dataConst().sensorSampleIntervalMs));
            return;
        }

        if (equals(tokens.value[1], "samples"))
        {
            if (tokens.count == 2)
            {
                replyList(shell, "bus_samples", "value:%u", DeskSettings::dataConst().sensorSampleCount);
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
            replyList(shell, "ok", "bus.samples:%u", DeskSettings::dataConst().sensorSampleCount);
            return;
        }

        reply(shell, "ERR use: bus scan|time_read|samples|sensor [value]");
    }

    void handleKeypadQuery(Shell &shell)
    {
        const KeypadPeripheral::Snapshot &keypad = KeypadPeripheral::snapshot();

        replyList(shell, "keypad", "state:%s, protocol:%s, event_pin:%s, state_pin:%s, irq:%s, irq_count:%lu, event_code:%u, action_id:%u, action:%s",
                  DeskBus::ready(DeskBus::Device::Keypad) ? "ok" : "fail",
                  keypad.protocolText[0] != '\0' ? keypad.protocolText : "unknown",
                  KeypadPeripheral::eventPending() ? "high" : "low",
                  keypad.statePin ? "high" : "low",
                  keypad.eventInterruptEnabled ? "on" : "off",
                  keypad.eventInterruptCount,
                  keypad.lastButtonEventCode,
                  keypad.lastActionId,
                  keypad.lastAction[0] != '\0' ? keypad.lastAction : "none");
        replyList(shell, "keypad_buttons", "pressed:%u, released:%u, down:%u, last:%u, edge:%u, assign:%u/%u/%u/%u/%u",
                  keypad.buttonPressedMask,
                  keypad.buttonReleasedMask,
                  keypad.buttonDownMask,
                  keypad.lastButton,
                  keypad.lastButtonEdge,
                  keypad.buttonAssignments[0],
                  keypad.buttonAssignments[1],
                  keypad.buttonAssignments[2],
                  keypad.buttonAssignments[3],
                  keypad.buttonAssignments[4]);
        replyList(shell, "keypad_light", "enabled:%s, mode:%u, brightness:%u, kelvin:%u, pwm_cold:%u, pwm_warm:%u, current_ma:%d, voltage_mv:%u, power_mw:%lu, raw_current:%u, raw_voltage:%u",
                  keypad.stripEnabled ? "yes" : "no",
                  keypad.stripMode,
                  keypad.brightness,
                  keypad.kelvin,
                  keypad.pwmCold,
                  keypad.pwmWarm,
                  keypad.currentMa,
                  keypad.supplyMv,
                  keypad.powerMw,
                  keypad.currentRaw,
                  keypad.voltageRaw);
    }

    void handleKeypad(Shell &shell, const Tokens &tokens)
    {
        if (tokens.count == 1)
        {
            handleKeypadQuery(shell);
            return;
        }

        if (equals(tokens.value[1], "action") || equals(tokens.value[1], "assign"))
        {
            if (tokens.count == 3 && equals(tokens.value[2], "list"))
            {
                replyList(shell, "keypad_actions", "0:none, 1:mute, 2:volume_up, 3:volume_down, 4:play_pause, 5:next, 6:previous, 7:f13, 8:f14, 9:f15");
                return;
            }

            if (tokens.count < 4)
            {
                reply(shell, "ERR use: keypad action <0..4> <none|mute|volume_up|volume_down|play_pause|next|previous|f13|f14|f15>");
                return;
            }

            uint32_t button = 0;
            uint8_t actionId = 0;
            if (!parseU32(tokens.value[2], button) || button >= 5 || !parseKeypadActionId(tokens.value[3], actionId))
            {
                reply(shell, "ERR invalid keypad action");
                return;
            }

            if (!KeypadPeripheral::setButtonAction(static_cast<uint8_t>(button), actionId))
            {
                reply(shell, "ERR keypad missing");
                return;
            }

            marker(shell, DeskMarker::KeypadActionUpdated, "keypad.action.updated");
            replyList(shell, "ok", "keypad.button:%lu, action_id:%u, action:%s",
                      static_cast<unsigned long>(button),
                      actionId,
                      keypadActionName(actionId));
            return;
        }

        reply(shell, "ERR use: keypad | keypad action list | keypad action <0..4> <action>");
    }

    void handleLightQuery(Shell &shell)
    {
        const KeypadPeripheral::Snapshot &keypad = KeypadPeripheral::snapshot();

        replyList(shell, "light", "enabled:%s, mode:%s, brightness:%u, brightness_min:%u, brightness_max:%u, kelvin:%u, cold_min:%u, cold_max:%u, warm_min:%u, warm_max:%u",
                  StripLight::enabled() ? "yes" : "no",
                  StripLight::modeName(),
                  StripLight::brightness(),
                  StripLight::brightnessMin(),
                  StripLight::brightnessMax(),
                  StripLight::kelvinMix(),
                  StripLight::coldMin(),
                  StripLight::coldMax(),
                  StripLight::hotMin(),
                  StripLight::hotMax());
        replyList(shell, "light_buttons", "click_ms:%u, long_ms:%u, repeat_ms:%u, brightness_step:%u, kelvin_step:%u",
                  StripLight::buttonClickTime(),
                  StripLight::buttonLongTime(),
                  StripLight::buttonDuringTime(),
                  StripLight::brightnessStep(),
                  StripLight::kelvinStep());
        replyList(shell, "light_power", "keypad:%s, pwm_cold:%u, pwm_warm:%u, current_ma:%d, raw_current:%u, voltage_mv:%u",
                  KeypadPeripheral::present() ? "ok" : "missing",
                  keypad.pwmCold,
                  keypad.pwmWarm,
                  keypad.currentMa,
                  keypad.currentRaw,
                  keypad.supplyMv);
    }

    void handleLight(Shell &shell, const Tokens &tokens)
    {
        if (tokens.count < 2)
        {
            reply(shell, "ERR use: light enabled|mode|brightness|kelvin|cold_min|cold_max|warm_min|warm_max|click|long|repeat|brightness_step|kelvin_step|sync|power [value]");
            return;
        }

        if (equals(tokens.value[1], "sync"))
        {
            if (KeypadPeripheral::syncStripConfig())
            {
                replyList(shell, "ok", "light.sync:done");
            }
            else
            {
                reply(shell, "ERR keypad missing");
            }
            return;
        }

        if (equals(tokens.value[1], "power"))
        {
            KeypadPeripheral::update();
            const KeypadPeripheral::Snapshot &keypad = KeypadPeripheral::snapshot();
            replyList(shell, "light_power", "state:%s, current_ma:%d, voltage_mv:%u, power_mw:%lu, raw_current:%u, raw_voltage:%u",
                      KeypadPeripheral::present() ? "ok" : "missing",
                      keypad.currentMa,
                      keypad.supplyMv,
                      keypad.powerMw,
                      keypad.currentRaw,
                      keypad.voltageRaw);
            return;
        }

        if (tokens.count < 3)
        {
            handleLightQuery(shell);
            return;
        }

        uint32_t value = 0;
        bool boolValue = false;

        if (equals(tokens.value[1], "enabled"))
        {
            if (!parseBoolValue(tokens.value[2], boolValue))
            {
                reply(shell, "ERR invalid enabled value");
                return;
            }
            StripLight::setEnabled(boolValue);
        }
        else if (equals(tokens.value[1], "mode"))
        {
            StripLight::Mode mode = StripLight::Mode::Composite;
            if (!parseStripMode(tokens.value[2], mode))
            {
                reply(shell, "ERR invalid mode value");
                return;
            }
            StripLight::setMode(mode);
        }
        else if (equals(tokens.value[1], "brightness"))
        {
            if (!parseU32(tokens.value[2], value))
            {
                reply(shell, "ERR invalid brightness value");
                return;
            }
            StripLight::setBrightness(clampU16(value, STRIP_BRIGHTNESS_MIN_DEFAULT, STRIP_PWM_MAX_DEFAULT));
            StripLight::setEnabled(true);
        }
        else if (equals(tokens.value[1], "kelvin"))
        {
            if (!parseU32(tokens.value[2], value))
            {
                reply(shell, "ERR invalid kelvin value");
                return;
            }
            StripLight::setKelvinMix(clampU16(value, 0, STRIP_PWM_MAX_DEFAULT));
            StripLight::setEnabled(true);
        }
        else if (equals(tokens.value[1], "cold_min"))
        {
            if (!parseU32(tokens.value[2], value)) { reply(shell, "ERR invalid cold_min value"); return; }
            StripLight::setColdMin(clampU16(value, 0, STRIP_PWM_MAX_DEFAULT));
        }
        else if (equals(tokens.value[1], "cold_max"))
        {
            if (!parseU32(tokens.value[2], value)) { reply(shell, "ERR invalid cold_max value"); return; }
            StripLight::setColdMax(clampU16(value, 0, STRIP_PWM_MAX_DEFAULT));
        }
        else if (equals(tokens.value[1], "warm_min"))
        {
            if (!parseU32(tokens.value[2], value)) { reply(shell, "ERR invalid warm_min value"); return; }
            StripLight::setHotMin(clampU16(value, 0, STRIP_PWM_MAX_DEFAULT));
        }
        else if (equals(tokens.value[1], "warm_max"))
        {
            if (!parseU32(tokens.value[2], value)) { reply(shell, "ERR invalid warm_max value"); return; }
            StripLight::setHotMax(clampU16(value, 0, STRIP_PWM_MAX_DEFAULT));
        }
        else if (equals(tokens.value[1], "click"))
        {
            if (!parseU32(tokens.value[2], value)) { reply(shell, "ERR invalid click value"); return; }
            StripLight::setButtonClickTime(clampU16(value, STRIP_BUTTON_CLICK_MS_MIN_DEFAULT, STRIP_BUTTON_CLICK_MS_MAX_DEFAULT));
        }
        else if (equals(tokens.value[1], "long"))
        {
            if (!parseU32(tokens.value[2], value)) { reply(shell, "ERR invalid long value"); return; }
            StripLight::setButtonLongTime(clampU16(value, STRIP_BUTTON_LONG_MS_MIN_DEFAULT, STRIP_BUTTON_LONG_MS_MAX_DEFAULT));
        }
        else if (equals(tokens.value[1], "repeat"))
        {
            if (!parseU32(tokens.value[2], value)) { reply(shell, "ERR invalid repeat value"); return; }
            StripLight::setButtonDuringTime(clampU16(value, STRIP_BUTTON_REPEAT_MS_MIN_DEFAULT, STRIP_BUTTON_REPEAT_MS_MAX_DEFAULT));
        }
        else if (equals(tokens.value[1], "brightness_step"))
        {
            if (!parseU32(tokens.value[2], value)) { reply(shell, "ERR invalid brightness_step value"); return; }
            StripLight::setBrightnessStep(clampU16(value, STRIP_STEP_MIN_DEFAULT, STRIP_STEP_MAX_DEFAULT));
        }
        else if (equals(tokens.value[1], "kelvin_step"))
        {
            if (!parseU32(tokens.value[2], value)) { reply(shell, "ERR invalid kelvin_step value"); return; }
            StripLight::setKelvinStep(clampU16(value, STRIP_STEP_MIN_DEFAULT, STRIP_STEP_MAX_DEFAULT));
        }
        else
        {
            reply(shell, "ERR unknown light field");
            return;
        }

        persistLightConfig();
        marker(shell, DeskMarker::LightConfigUpdated, "light.config.updated");
        logLine(shell, "light: config updated");
        replyList(shell, "ok", "light.updated:done");
    }

    void handleReset(Shell &shell, const Tokens &tokens)
    {
        const char *target = tokens.count >= 2 ? tokens.value[1] : "peripherals";

        if (equals(target, "keypad"))
        {
            KeypadPeripheral::hardwareReset();
            marker(shell, DeskMarker::ResetKeypad, "reset.keypad");
            logLine(shell, "reset: keypad");
            replyList(shell, "ok", "reset:keypad");
            return;
        }

        if (equals(target, "peripherals") || equals(target, "periferics"))
        {
            KeypadPeripheral::hardwareReset();
            marker(shell, DeskMarker::ResetKeypad, "reset.peripherals.keypad");
            logLine(shell, "reset: peripherals");
            replyList(shell, "ok", "reset:peripherals, keypad:done");
            return;
        }

        if (equals(target, "settings"))
        {
            DeskSettings::resetDefaults();
            DeskSettings::saveNow();
            KeypadPeripheral::syncStripConfig();
            marker(shell, DeskMarker::ResetSettings, "reset.settings");
            logLine(shell, "reset: settings defaults");
            replyList(shell, "ok", "reset:settings");
            return;
        }

        if (equals(target, "core") || equals(target, "mcu"))
        {
            marker(shell, DeskMarker::ResetCore, "reset.core");
            logLine(shell, "reset: core");
            replyList(shell, "ok", "reset:core");
            delay(20);
#if defined(ARDUINO_ARCH_ESP32)
            ESP.restart();
#else
            NVIC_SystemReset();
#endif
            return;
        }

        reply(shell, "ERR use: reset [peripherals|keypad|settings|core]");
    }

    void handleSensorQuery(Shell &shell)
    {
        replyList(shell, "bus_sensors", "0:ENS160, 1:AHT21, 2:VEML7700, 3:SCD41, 4:SHT21");
    }

    void handleRtcQuery(Shell &shell)
    {
        replyList(shell, "rtc", "state:%s, lost_pwr:%s",
                  DeskBus::ready(DeskBus::Device::DS3231) ? "ok" : "fail",
                  DeskBus::rtcLostPower() ? "yes" : "no");
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
            replyList(shell, "rtc_time", "value:%04u-%02u-%02uT%02u:%02u:%02u", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
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

            if (DeskBus::setRtc(value))
            {
                replyList(shell, "ok", "rtc.set:done");
            }
            else
            {
                reply(shell, "ERR rtc not ready");
            }
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
            replyList(shell, "ping", "state:pong");
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

        if (equals(tokens.value[0], "display?"))
        {
            handleDisplayQuery(shell);
            return true;
        }

        if (equals(tokens.value[0], "display"))
        {
            handleDisplay(shell, tokens);
            return true;
        }

        if (equals(tokens.value[0], "wireless?"))
        {
            handleWirelessQuery(shell);
            return true;
        }

        if (equals(tokens.value[0], "wireless"))
        {
            handleWireless(shell, tokens);
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

        if (equals(tokens.value[0], "keypad?"))
        {
            handleKeypadQuery(shell);
            return true;
        }

        if (equals(tokens.value[0], "keypad"))
        {
            handleKeypad(shell, tokens);
            return true;
        }

        if (equals(tokens.value[0], "light?"))
        {
            handleLightQuery(shell);
            return true;
        }

        if (equals(tokens.value[0], "light"))
        {
            handleLight(shell, tokens);
            return true;
        }

        if (equals(tokens.value[0], "reset"))
        {
            handleReset(shell, tokens);
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
