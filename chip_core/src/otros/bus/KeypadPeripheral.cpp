#include "bus/KeypadPeripheral.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

#include "config/DeskConfig.h"
#include "config/pins_config.h"
#include "light/StripLight.h"

namespace
{
    KeypadPeripheral::Snapshot current;
    volatile bool eventInterruptFlag = false;
    volatile uint32_t eventInterruptCount = 0;
    bool buttonEventNotificationPending = false;

#if KEYPAD_UART_CONFIGURED
    HardwareSerial keypadSerial(KEYPAD_UART_RX_PIN, KEYPAD_UART_TX_PIN);
#endif

    void handleEventInterrupt()
    {
        eventInterruptFlag = true;
        ++eventInterruptCount;
    }

    bool resetPinAvailable()
    {
        return KEYPAD_RESET_PIN != NC;
    }

    void clearInput()
    {
#if KEYPAD_UART_CONFIGURED
        while (keypadSerial.available() > 0)
        {
            keypadSerial.read();
        }
#endif
    }

    bool readLine(char *buffer, uint8_t capacity)
    {
#if !KEYPAD_UART_CONFIGURED
        (void)buffer;
        (void)capacity;
        return false;
#else
        uint8_t length = 0;
        const uint32_t startMs = millis();

        while (millis() - startMs < KEYPAD_UART_TIMEOUT_MS_DEFAULT)
        {
            while (keypadSerial.available() > 0)
            {
                const char value = static_cast<char>(keypadSerial.read());
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
#endif
    }

    bool sendLine(const char *line, char *response, uint8_t responseCapacity)
    {
#if !KEYPAD_UART_CONFIGURED
        (void)line;
        (void)response;
        (void)responseCapacity;
        return false;
#else
        clearInput();
        keypadSerial.print(line);
        keypadSerial.print('\n');
        keypadSerial.flush();

        if (!readLine(response, responseCapacity))
        {
            return false;
        }

        return strncmp(response, "OK", 2) == 0;
#endif
    }

    bool sendCommand(const char *line)
    {
        char response[48] = {};
        return sendLine(line, response, sizeof(response));
    }

    bool sendCommandValue(const char *prefix, uint16_t value)
    {
        char command[20] = {};
        snprintf(command, sizeof(command), "%s%u", prefix, value);
        return sendCommand(command);
    }

    bool stringForKey(const char *line, const char *key, char *value, uint8_t valueCapacity)
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

    bool valueForKey(const char *line, const char *key, long &value)
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

    bool boolForKey(const char *line, const char *key, bool &value)
    {
        long numeric = 0;
        if (!valueForKey(line, key, numeric))
        {
            return false;
        }

        value = numeric != 0;
        return true;
    }

    void parseStripResponse(const char *response)
    {
        long numeric = 0;
        bool enabled = false;

        if (boolForKey(response, "EN=", enabled))
        {
            current.stripEnabled = enabled;
        }
        if (valueForKey(response, "MODE=", numeric))
        {
            current.stripMode = static_cast<uint8_t>(numeric);
        }
        if (valueForKey(response, "VAL=", numeric))
        {
            current.brightness = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "KEL=", numeric))
        {
            current.kelvin = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "CMIN=", numeric))
        {
            current.coldMin = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "CMAX=", numeric))
        {
            current.coldMax = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "WMIN=", numeric))
        {
            current.warmMin = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "WMAX=", numeric))
        {
            current.warmMax = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "BSTEP=", numeric))
        {
            current.brightnessStep = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "KSTEP=", numeric))
        {
            current.kelvinStep = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "CT=", numeric))
        {
            current.clickMs = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "LT=", numeric))
        {
            current.longMs = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "RT=", numeric))
        {
            current.repeatMs = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "PWM_C=", numeric))
        {
            current.pwmCold = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "PWM_W=", numeric))
        {
            current.pwmWarm = static_cast<uint16_t>(numeric);
        }
    }

    void parsePowerResponse(const char *response)
    {
        long numeric = 0;

        if (valueForKey(response, "MA=", numeric))
        {
            current.currentMa = static_cast<int16_t>(numeric);
        }
        if (valueForKey(response, "RAW=", numeric))
        {
            current.currentRaw = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "MV=", numeric))
        {
            current.supplyMv = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "VRAW=", numeric))
        {
            current.voltageRaw = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "MW=", numeric))
        {
            current.powerMw = static_cast<uint32_t>(numeric);
        }
    }

    void parseInfoResponse(const char *response)
    {
        stringForKey(response, "PROTO=", current.protocolText, sizeof(current.protocolText));
    }

    void parseEventResponse(const char *response)
    {
        long numeric = 0;

        if (valueForKey(response, "EVT=", numeric))
        {
            current.events = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "BP=", numeric))
        {
            current.buttonPressedMask = static_cast<uint8_t>(numeric);
        }
        if (valueForKey(response, "BR=", numeric))
        {
            current.buttonReleasedMask = static_cast<uint8_t>(numeric);
        }
        if (valueForKey(response, "BD=", numeric))
        {
            current.buttonDownMask = static_cast<uint8_t>(numeric);
        }
        if (valueForKey(response, "LB=", numeric))
        {
            current.lastButton = static_cast<uint8_t>(numeric);
        }
        if (valueForKey(response, "BE=", numeric))
        {
            current.lastButtonEdge = static_cast<uint8_t>(numeric);
        }

        if ((current.events & KeypadPeripheral::EventButtonEdge) != 0 ||
            current.buttonPressedMask != 0 ||
            current.buttonReleasedMask != 0)
        {
            buttonEventNotificationPending = true;
        }
    }

    void parseActionResponse(const char *response)
    {
        char action[9] = {};
        if (stringForKey(response, "ACT=", action, sizeof(action)) && strcmp(action, "NONE") != 0)
        {
            strncpy(current.lastAction, action, sizeof(current.lastAction) - 1);
            current.lastAction[sizeof(current.lastAction) - 1] = '\0';
        }
    }

    void parseButtonAssignmentsResponse(const char *response)
    {
        char key[5] = {};
        long numeric = 0;

        for (uint8_t index = 0; index < 5; ++index)
        {
            snprintf(key, sizeof(key), "B%u=", index);
            if (valueForKey(response, key, numeric))
            {
                current.buttonAssignments[index] = static_cast<uint8_t>(numeric);
            }
        }
    }

    bool rawInterruptFlagPending()
    {
        noInterrupts();
        const bool pending = eventInterruptFlag;
        interrupts();
        return pending;
    }

    uint8_t stripModeForKeypad()
    {
        switch (StripLight::mode())
        {
            case StripLight::Mode::OneSimple:
                return 0;
            case StripLight::Mode::TwoSimple:
                return 1;
            case StripLight::Mode::Composite:
            default:
                return 2;
        }
    }
}

namespace KeypadPeripheral
{
    void prepareResetHold()
    {
        if (resetPinAvailable())
        {
            pinMode(KEYPAD_RESET_PIN, OUTPUT);
            digitalWrite(KEYPAD_RESET_PIN, LOW);
        }

        pinMode(KEYPAD_EVENT_PIN, INPUT_PULLDOWN);
        pinMode(KEYPAD_STATE_PIN, INPUT_PULLDOWN);
        eventInterruptFlag = false;
        eventInterruptCount = 0;
    }

    void begin()
    {
        if (resetPinAvailable())
        {
            pinMode(KEYPAD_RESET_PIN, OUTPUT);
            digitalWrite(KEYPAD_RESET_PIN, LOW);
            delay(KEYPAD_RESET_PULSE_MS_DEFAULT);
            digitalWrite(KEYPAD_RESET_PIN, HIGH);
        }
        delay(KEYPAD_BOOT_DELAY_MS_DEFAULT);

        pinMode(KEYPAD_EVENT_PIN, INPUT_PULLDOWN);
        pinMode(KEYPAD_STATE_PIN, INPUT_PULLDOWN);
        eventInterruptFlag = false;
        eventInterruptCount = 0;
        current.eventInterruptEnabled = true;
        attachInterrupt(digitalPinToInterrupt(KEYPAD_EVENT_PIN), handleEventInterrupt, RISING);

#if KEYPAD_UART_CONFIGURED
        keypadSerial.begin(KEYPAD_UART_BAUD_DEFAULT);
        clearInput();
#endif

        update();
        syncStripConfig();
    }

    void update()
    {
        char response[160] = {};
        current.present = sendLine("PING", response, sizeof(response));
        if (!current.present)
        {
            current.protocol = 0;
            return;
        }

        current.protocol = KEYPAD_PROTOCOL_VERSION_DEFAULT;
        current.statePin = digitalRead(KEYPAD_STATE_PIN) == HIGH;

        if (sendLine("INF?", response, sizeof(response)))
        {
            parseInfoResponse(response);
        }

        serviceEvents();

        if (sendLine("STR?", response, sizeof(response)))
        {
            parseStripResponse(response);
        }

        if (sendLine("PWR?", response, sizeof(response)))
        {
            parsePowerResponse(response);
        }

        if (sendLine("BAS?", response, sizeof(response)))
        {
            parseButtonAssignmentsResponse(response);
        }
    }

    void serviceEvents()
    {
        if (!rawInterruptFlagPending() && !eventPending())
        {
            return;
        }

        noInterrupts();
        eventInterruptFlag = false;
        current.eventInterruptCount = eventInterruptCount;
        interrupts();

        char response[160] = {};
        if (!sendLine("EVT?", response, sizeof(response)))
        {
            current.present = false;
            return;
        }

        current.present = true;
        parseEventResponse(response);

        if ((current.events & EventCoreCommand) != 0)
        {
            if (sendLine("ACT?", response, sizeof(response)))
            {
                parseActionResponse(response);
            }
        }
    }

    bool present()
    {
        return current.present;
    }

    bool eventPending()
    {
        return digitalRead(KEYPAD_EVENT_PIN) == HIGH;
    }

    bool stateActive()
    {
        current.statePin = digitalRead(KEYPAD_STATE_PIN) == HIGH;
        return current.statePin;
    }

    bool consumeInterruptFlag()
    {
        noInterrupts();
        const bool pending = eventInterruptFlag;
        eventInterruptFlag = false;
        current.eventInterruptCount = eventInterruptCount;
        interrupts();
        return pending;
    }

    bool consumeButtonEventNotification()
    {
        const bool pending = buttonEventNotificationPending;
        buttonEventNotificationPending = false;
        return pending;
    }

    void hardwareReset()
    {
        if (!resetPinAvailable())
        {
            return;
        }

        digitalWrite(KEYPAD_RESET_PIN, LOW);
        delay(KEYPAD_RESET_PULSE_MS_DEFAULT);
        digitalWrite(KEYPAD_RESET_PIN, HIGH);
        delay(KEYPAD_BOOT_DELAY_MS_DEFAULT);
        clearInput();
        update();
        syncStripConfig();
    }

    void clearEvents()
    {
        sendCommand("SCL");
        current.events = 0;
        current.status &= ~0x02;
    }

    bool command(uint8_t commandValue)
    {
        switch (commandValue)
        {
            case 1:
                return sendCommand("SCL");
            case 2:
                return sendCommand("SRD");
            case 3:
                return sendCommand(current.stripEnabled ? "SSE0" : "SSE1");
            case 4:
                return sendCommand("SSK0");
            case 5:
                return sendCommandValue("SSK", StripLight::pwmMax() / 2);
            case 6:
                return sendCommandValue("SSK", StripLight::pwmMax());
            default:
                return false;
        }
    }

    bool writeU16(uint8_t regLow, uint16_t value)
    {
        switch (regLow)
        {
            case 0x08:
                return sendCommandValue("SSV", value);
            case 0x0A:
                return sendCommandValue("SSK", value);
            case 0x0C:
                return sendCommandValue("SCN", value);
            case 0x0E:
                return sendCommandValue("SCX", value);
            case 0x10:
                return sendCommandValue("SWN", value);
            case 0x12:
                return sendCommandValue("SWX", value);
            case 0x14:
                return sendCommandValue("SBS", value);
            case 0x16:
                return sendCommandValue("SKS", value);
            case 0x1A:
                return sendCommandValue("SCT", value);
            case 0x1C:
                return sendCommandValue("SLT", value);
            case 0x1E:
                return sendCommandValue("SRT", value);
            default:
                return false;
        }
    }

    bool setButtonAction(uint8_t buttonIndex, uint8_t actionId)
    {
        if (buttonIndex >= 5)
        {
            return false;
        }

        char commandBuffer[16] = {};
        char response[48] = {};
        snprintf(commandBuffer, sizeof(commandBuffer), "BSA%u,%u", buttonIndex, actionId);
        if (!sendLine(commandBuffer, response, sizeof(response)))
        {
            return false;
        }

        current.buttonAssignments[buttonIndex] = actionId;
        return true;
    }

    bool syncStripConfig()
    {
        bool ok = true;
        ok &= sendCommand(StripLight::enabled() ? "SSE1" : "SSE0");
        ok &= sendCommandValue("SSM", stripModeForKeypad());
        ok &= sendCommandValue("SSV", StripLight::brightness());
        ok &= sendCommandValue("SSK", StripLight::kelvinMix());
        ok &= sendCommandValue("SCN", StripLight::coldMin());
        ok &= sendCommandValue("SCX", StripLight::coldMax());
        ok &= sendCommandValue("SWN", StripLight::hotMin());
        ok &= sendCommandValue("SWX", StripLight::hotMax());
        ok &= sendCommandValue("SBS", StripLight::brightnessStep());
        ok &= sendCommandValue("SKS", StripLight::kelvinStep());
        ok &= sendCommandValue("SCT", StripLight::buttonClickTime());
        ok &= sendCommandValue("SLT", StripLight::buttonLongTime());
        ok &= sendCommandValue("SRT", StripLight::buttonDuringTime());
        update();
        return ok;
    }

    const Snapshot &snapshot()
    {
        return current;
    }
}
