#pragma once

#include <Arduino.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace DeskBridgeKeypadProtocol
{
    constexpr uint8_t ProtocolVersion = 0x03;
    constexpr const char *ProtocolText = "TXT1";
    constexpr uint8_t DeviceId = 66;
    constexpr const char *DeviceName = "CHIP_KEYPAD";
    constexpr const char *FirmwareVersion = "0.3.0";
    constexpr uint8_t ButtonCount = 5;

    namespace Command
    {
        constexpr const char *Ping = "PING";
        constexpr const char *Info = "INF?";
        constexpr const char *Strip = "STR?";
        constexpr const char *Power = "PWR?";
        constexpr const char *Events = "EVT?";
        constexpr const char *Buttons = "BTN?";
        constexpr const char *Action = "ACT?";
        constexpr const char *ButtonActionList = "BAL?";
        constexpr const char *ButtonAssignments = "BAS?";
        constexpr const char *ClearEvents = "SCL";
        constexpr const char *ResetDefaults = "SRD";
    }

    namespace Prefix
    {
        constexpr const char *SetEnabled = "SSE";
        constexpr const char *SetMode = "SSM";
        constexpr const char *SetBrightness = "SSV";
        constexpr const char *SetKelvin = "SSK";
        constexpr const char *SetColdMin = "SCN";
        constexpr const char *SetColdMax = "SCX";
        constexpr const char *SetWarmMin = "SWN";
        constexpr const char *SetWarmMax = "SWX";
        constexpr const char *SetBrightnessStep = "SBS";
        constexpr const char *SetKelvinStep = "SKS";
        constexpr const char *SetClickMs = "SCT";
        constexpr const char *SetLongMs = "SLT";
        constexpr const char *SetRepeatMs = "SRT";
        constexpr const char *SetButtonAction = "BSA";
    }

    namespace Response
    {
        constexpr const char *Ok = "OK";
        constexpr const char *Err = "ERR";
        constexpr const char *Pong = "OK PONG";
        constexpr const char *ActionNone = "NONE";
    }

    namespace Field
    {
        constexpr const char *Device = "DEV=";
        constexpr const char *Protocol = "PROTO=";
        constexpr const char *Name = "NAME=";
        constexpr const char *Firmware = "FW=";
        constexpr const char *ProtocolVersion = "PV=";
        constexpr const char *Enabled = "EN=";
        constexpr const char *Mode = "MODE=";
        constexpr const char *Brightness = "VAL=";
        constexpr const char *Kelvin = "KEL=";
        constexpr const char *ColdMin = "CMIN=";
        constexpr const char *ColdMax = "CMAX=";
        constexpr const char *WarmMin = "WMIN=";
        constexpr const char *WarmMax = "WMAX=";
        constexpr const char *BrightnessStep = "BSTEP=";
        constexpr const char *KelvinStep = "KSTEP=";
        constexpr const char *ClickMs = "CT=";
        constexpr const char *LongMs = "LT=";
        constexpr const char *RepeatMs = "RT=";
        constexpr const char *PwmCold = "PWM_C=";
        constexpr const char *PwmWarm = "PWM_W=";
        constexpr const char *CurrentMa = "MA=";
        constexpr const char *CurrentRaw = "RAW=";
        constexpr const char *SupplyMv = "MV=";
        constexpr const char *VoltageRaw = "VRAW=";
        constexpr const char *PowerMw = "MW=";
        constexpr const char *Events = "EVT=";
        constexpr const char *ButtonPressed = "BP=";
        constexpr const char *ButtonReleased = "BR=";
        constexpr const char *ButtonDown = "BD=";
        constexpr const char *LastButton = "LB=";
        constexpr const char *ButtonEdge = "BE=";
        constexpr const char *ButtonEventCode = "EC=";
        constexpr const char *ActionId = "AID=";
        constexpr const char *Action = "ACT=";
        constexpr const char *AvailableCount = "N=";
    }

    inline bool startsWith(const char *text, const char *prefix)
    {
        return strncmp(text, prefix, strlen(prefix)) == 0;
    }

    inline bool isOk(const char *line)
    {
        return strncmp(line, Response::Ok, strlen(Response::Ok)) == 0;
    }

    inline bool valueForKey(const char *line, const char *key, long &value)
    {
        const char *cursor = strstr(line, key);
        if (cursor == nullptr)
        {
            return false;
        }

        cursor += strlen(key);
        value = strtol(cursor, nullptr, 10);
        return true;
    }

    inline bool boolForKey(const char *line, const char *key, bool &value)
    {
        long numeric = 0;
        if (!valueForKey(line, key, numeric))
        {
            return false;
        }

        value = numeric != 0;
        return true;
    }

    inline bool stringForKey(const char *line, const char *key, char *value, uint8_t valueCapacity)
    {
        if (valueCapacity == 0)
        {
            return false;
        }

        const char *cursor = strstr(line, key);
        if (cursor == nullptr)
        {
            return false;
        }

        cursor += strlen(key);
        uint8_t index = 0;
        while (*cursor != '\0' && *cursor != ' ' && index + 1 < valueCapacity)
        {
            value[index++] = *cursor++;
        }
        value[index] = '\0';
        return index > 0;
    }

    inline bool readLine(Stream &stream, char *buffer, uint8_t capacity, uint32_t timeoutMs)
    {
        if (capacity == 0)
        {
            return false;
        }

        uint8_t length = 0;
        const uint32_t startMs = millis();

        while (millis() - startMs < timeoutMs)
        {
            while (stream.available() > 0)
            {
                const char value = static_cast<char>(stream.read());
                if (value == '\r')
                {
                    continue;
                }

                if (value == '\n')
                {
                    buffer[length] = '\0';
                    return length > 0;
                }

                if (length + 1 < capacity)
                {
                    buffer[length++] = value;
                }
            }
        }

        buffer[0] = '\0';
        return false;
    }

    inline void clearInput(Stream &stream)
    {
        while (stream.available() > 0)
        {
            stream.read();
        }
    }

    inline bool sendLine(HardwareSerial &serial, const char *line, char *response, uint8_t responseCapacity, uint32_t timeoutMs)
    {
        clearInput(serial);
        serial.print(line);
        serial.print('\n');
        serial.flush();

        return readLine(serial, response, responseCapacity, timeoutMs) && isOk(response);
    }

    inline bool formatValueCommand(char *buffer, uint8_t capacity, const char *prefix, uint16_t value)
    {
        return snprintf(buffer, capacity, "%s%u", prefix, value) > 0;
    }

    inline bool formatBoolCommand(char *buffer, uint8_t capacity, const char *prefix, bool value)
    {
        return snprintf(buffer, capacity, "%s%u", prefix, value ? 1 : 0) > 0;
    }

    inline bool formatButtonAssignmentCommand(char *buffer, uint8_t capacity, uint8_t buttonIndex, uint8_t actionId)
    {
        return snprintf(buffer, capacity, "%s%u,%u", Prefix::SetButtonAction, buttonIndex, actionId) > 0;
    }
}
