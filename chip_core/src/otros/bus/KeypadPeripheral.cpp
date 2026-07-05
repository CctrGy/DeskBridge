#include "bus/KeypadPeripheral.h"

#include <Arduino.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config/DeskConfig.h"
#include "config/pins_config.h"
#include "light/StripLight.h"

namespace
{
    constexpr uint8_t lineCapacity = 180;
    constexpr uint8_t shortLineCapacity = 64;

    KeypadPeripheral::Snapshot current;
    volatile bool eventInterruptFlag = false;
    volatile uint32_t eventInterruptCount = 0;
    bool buttonEventNotificationPending = false;

#if defined(ARDUINO_ARCH_ESP32)
    HardwareSerial keypadSerial(1);
#else
    HardwareSerial keypadSerial(KEYPAD_UART_RX_PIN, KEYPAD_UART_TX_PIN);
#endif

    bool serialPinsAvailable()
    {
        return KEYPAD_UART_RX_PIN != NC && KEYPAD_UART_TX_PIN != NC;
    }

    bool eventPinAvailable()
    {
        return KEYPAD_EVENT_PIN != NC;
    }

    bool statePinAvailable()
    {
        return KEYPAD_STATE_PIN != NC;
    }

    void handleEventInterrupt()
    {
        eventInterruptFlag = true;
        ++eventInterruptCount;
        buttonEventNotificationPending = true;
    }

    bool resetPinAvailable()
    {
        return KEYPAD_RESET_PIN != NC;
    }

    bool startsWith(const char *text, const char *prefix)
    {
        return strncmp(text, prefix, strlen(prefix)) == 0;
    }

    bool isOk(const char *line)
    {
        return startsWith(line, "OK");
    }

    bool valueForKey(const char *line, const char *key, long &value)
    {
        const char *cursor = strstr(line, key);
        if (cursor == nullptr)
        {
            return false;
        }

        cursor += strlen(key);
        value = strtol(cursor, nullptr, 0);
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
        while (*cursor != '\0' && !isspace(static_cast<unsigned char>(*cursor)) && index + 1 < valueCapacity)
        {
            value[index++] = *cursor++;
        }
        value[index] = '\0';
        return index > 0;
    }

    void clearInput()
    {
        if (!serialPinsAvailable())
        {
            return;
        }

        while (keypadSerial.available() > 0)
        {
            keypadSerial.read();
        }
    }

    bool readLine(char *buffer, uint8_t capacity)
    {
        if (capacity == 0)
        {
            return false;
        }

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
    }

    bool sendLine(const char *line, char *response, uint8_t responseCapacity)
    {
        if (!serialPinsAvailable())
        {
            if (responseCapacity > 0)
            {
                response[0] = '\0';
            }
            return false;
        }

        clearInput();
        keypadSerial.print(line);
        keypadSerial.print('\n');
        keypadSerial.flush();

        return readLine(response, responseCapacity) && isOk(response);
    }

    bool sendCommand(const char *line)
    {
        char response[shortLineCapacity] = {};
        return sendLine(line, response, sizeof(response));
    }

    bool sendSetCommand(const char *section, const char *field, uint16_t value)
    {
        char command[48] = {};
        snprintf(command, sizeof(command), "%s SET %s %u", section, field, value);
        return sendCommand(command);
    }

    bool sendSetCommand(const char *section, const char *field, bool value)
    {
        return sendSetCommand(section, field, static_cast<uint16_t>(value ? 1 : 0));
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

    void configureSignalPins()
    {
        if (eventPinAvailable())
        {
            pinMode(KEYPAD_EVENT_PIN, INPUT_PULLDOWN);
        }
        if (statePinAvailable())
        {
            pinMode(KEYPAD_STATE_PIN, INPUT_PULLDOWN);
        }
        current.eventInterruptEnabled = eventPinAvailable();
    }

    void updateSignalSnapshot()
    {
        current.statePin = statePinAvailable() && digitalRead(KEYPAD_STATE_PIN) == HIGH;
        if (current.statePin)
        {
            current.buttonDownMask = 0x01;
        }
        else
        {
            current.buttonDownMask = 0;
        }
    }

    bool rawInterruptFlagPending()
    {
        if (!eventPinAvailable())
        {
            return false;
        }

        noInterrupts();
        const bool pending = eventInterruptFlag;
        interrupts();
        return pending;
    }

    void parseInfoResponse(const char *response)
    {
        stringForKey(response, "PROTO=", current.protocolText, sizeof(current.protocolText));
        long numeric = 0;
        if (valueForKey(response, "PV=", numeric))
        {
            current.protocol = static_cast<uint8_t>(numeric);
        }
        else
        {
            current.protocol = KEYPAD_PROTOCOL_VERSION_DEFAULT;
        }
    }

    void parseLightResponse(const char *response)
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
        if (valueForKey(response, "BRIGHTNESS=", numeric) || valueForKey(response, "VAL=", numeric))
        {
            current.brightness = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "KELVIN=", numeric) || valueForKey(response, "KEL=", numeric))
        {
            current.kelvin = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "COLD_MIN=", numeric) || valueForKey(response, "CMIN=", numeric))
        {
            current.coldMin = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "COLD_MAX=", numeric) || valueForKey(response, "CMAX=", numeric))
        {
            current.coldMax = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "WARM_MIN=", numeric) || valueForKey(response, "WMIN=", numeric))
        {
            current.warmMin = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "WARM_MAX=", numeric) || valueForKey(response, "WMAX=", numeric))
        {
            current.warmMax = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "BRIGHTNESS_STEP=", numeric) || valueForKey(response, "BSTEP=", numeric))
        {
            current.brightnessStep = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "KELVIN_STEP=", numeric) || valueForKey(response, "KSTEP=", numeric))
        {
            current.kelvinStep = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "CLICK_MS=", numeric) || valueForKey(response, "CT=", numeric))
        {
            current.clickMs = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "LONG_MS=", numeric) || valueForKey(response, "LT=", numeric))
        {
            current.longMs = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "REPEAT_MS=", numeric) || valueForKey(response, "RT=", numeric))
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
        if (valueForKey(response, "RAW_I=", numeric) || valueForKey(response, "RAW=", numeric))
        {
            current.currentRaw = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "MV=", numeric))
        {
            current.supplyMv = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "RAW_V=", numeric) || valueForKey(response, "VRAW=", numeric))
        {
            current.voltageRaw = static_cast<uint16_t>(numeric);
        }
        if (valueForKey(response, "MW=", numeric))
        {
            current.powerMw = static_cast<uint32_t>(numeric);
        }
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
        if (valueForKey(response, "EC=", numeric))
        {
            current.lastButtonEventCode = static_cast<uint8_t>(numeric);
        }
        if (valueForKey(response, "AID=", numeric))
        {
            current.lastActionId = static_cast<uint8_t>(numeric);
        }
        if (stringForKey(response, "ACT=", current.lastAction, sizeof(current.lastAction)) &&
            strcmp(current.lastAction, "NONE") == 0)
        {
            current.lastAction[0] = '\0';
        }

        if ((current.events & KeypadPeripheral::EventButtonEdge) != 0 ||
            current.buttonPressedMask != 0 ||
            current.buttonReleasedMask != 0)
        {
            buttonEventNotificationPending = true;
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

    void queryEvents()
    {
        char response[lineCapacity] = {};
        if (!sendLine("EVENT?", response, sizeof(response)))
        {
            current.present = false;
            return;
        }

        current.present = true;
        parseEventResponse(response);
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

        configureSignalPins();
        eventInterruptFlag = false;
        eventInterruptCount = 0;
        buttonEventNotificationPending = false;
        current.present = false;
        current.protocol = 0;
        current.protocolText[0] = '\0';
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

        configureSignalPins();
        eventInterruptFlag = false;
        eventInterruptCount = 0;
        buttonEventNotificationPending = false;
        if (eventPinAvailable())
        {
            attachInterrupt(digitalPinToInterrupt(KEYPAD_EVENT_PIN), handleEventInterrupt, RISING);
        }

#if defined(ARDUINO_ARCH_ESP32)
        if (serialPinsAvailable())
        {
            keypadSerial.begin(KEYPAD_UART_BAUD_DEFAULT, SERIAL_8N1, KEYPAD_UART_RX_PIN, KEYPAD_UART_TX_PIN);
        }
#else
        keypadSerial.begin(KEYPAD_UART_BAUD_DEFAULT);
#endif
        clearInput();

        update();
        syncStripConfig();
    }

    void update()
    {
        if (!serialPinsAvailable())
        {
            current.present = false;
            current.protocol = 0;
            current.protocolText[0] = '\0';
            updateSignalSnapshot();
            return;
        }

        char response[lineCapacity] = {};
        current.present = sendLine("PING", response, sizeof(response));
        updateSignalSnapshot();
        if (!current.present)
        {
            current.protocol = 0;
            current.protocolText[0] = '\0';
            return;
        }

        current.protocol = KEYPAD_PROTOCOL_VERSION_DEFAULT;
        strncpy(current.protocolText, KEYPAD_PROTOCOL_TEXT_DEFAULT, sizeof(current.protocolText) - 1);
        current.protocolText[sizeof(current.protocolText) - 1] = '\0';

        if (sendLine("INFO?", response, sizeof(response)))
        {
            parseInfoResponse(response);
        }

        serviceEvents();

        if (sendLine("LIGHT?", response, sizeof(response)))
        {
            parseLightResponse(response);
        }

        if (sendLine("POWER?", response, sizeof(response)))
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

        queryEvents();
        updateSignalSnapshot();
    }

    bool present()
    {
        return current.present;
    }

    bool eventPending()
    {
        return eventPinAvailable() && digitalRead(KEYPAD_EVENT_PIN) == HIGH;
    }

    bool stateActive()
    {
        current.statePin = statePinAvailable() && digitalRead(KEYPAD_STATE_PIN) == HIGH;
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
        sendCommand("EVENT CLEAR");
        current.events = 0;
        current.buttonPressedMask = 0;
        current.buttonReleasedMask = 0;
        current.buttonDownMask = 0;
        current.lastButton = 0xFF;
        current.lastButtonEdge = 0;
        current.lastButtonEventCode = 0;
        current.lastActionId = 0;
        current.lastAction[0] = '\0';
        buttonEventNotificationPending = false;
    }

    bool command(uint8_t commandValue)
    {
        switch (commandValue)
        {
            case 1:
                return sendCommand("EVENT CLEAR");
            case 2:
                return sendCommand("CONFIG DEFAULTS");
            case 3:
                return sendSetCommand("LIGHT", "ENABLED", !current.stripEnabled);
            case 4:
                return sendSetCommand("LIGHT", "KELVIN", static_cast<uint16_t>(0));
            case 5:
                return sendSetCommand("LIGHT", "KELVIN", static_cast<uint16_t>(StripLight::pwmMax() / 2));
            case 6:
                return sendSetCommand("LIGHT", "KELVIN", StripLight::pwmMax());
            default:
                return false;
        }
    }

    bool writeU16(uint8_t regLow, uint16_t value)
    {
        switch (regLow)
        {
            case 0x08:
                return sendSetCommand("LIGHT", "BRIGHTNESS", value);
            case 0x0A:
                return sendSetCommand("LIGHT", "KELVIN", value);
            case 0x0C:
                return sendSetCommand("LIGHT", "COLD_MIN", value);
            case 0x0E:
                return sendSetCommand("LIGHT", "COLD_MAX", value);
            case 0x10:
                return sendSetCommand("LIGHT", "WARM_MIN", value);
            case 0x12:
                return sendSetCommand("LIGHT", "WARM_MAX", value);
            case 0x14:
                return sendSetCommand("LIGHT", "BRIGHTNESS_STEP", value);
            case 0x16:
                return sendSetCommand("LIGHT", "KELVIN_STEP", value);
            case 0x1A:
                return sendSetCommand("BUTTONS", "CLICK_MS", value);
            case 0x1C:
                return sendSetCommand("BUTTONS", "LONG_MS", value);
            case 0x1E:
                return sendSetCommand("BUTTONS", "REPEAT_MS", value);
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
        snprintf(commandBuffer, sizeof(commandBuffer), "BSA%u,%u", buttonIndex, actionId);
        if (!sendCommand(commandBuffer))
        {
            return false;
        }

        current.buttonAssignments[buttonIndex] = actionId;
        return true;
    }

    bool syncStripConfig()
    {
        bool ok = true;
        ok &= sendSetCommand("LIGHT", "ENABLED", StripLight::enabled());
        ok &= sendSetCommand("LIGHT", "MODE", static_cast<uint16_t>(stripModeForKeypad()));
        ok &= sendSetCommand("LIGHT", "BRIGHTNESS", StripLight::brightness());
        ok &= sendSetCommand("LIGHT", "KELVIN", StripLight::kelvinMix());
        ok &= sendSetCommand("LIGHT", "COLD_MIN", StripLight::coldMin());
        ok &= sendSetCommand("LIGHT", "COLD_MAX", StripLight::coldMax());
        ok &= sendSetCommand("LIGHT", "WARM_MIN", StripLight::hotMin());
        ok &= sendSetCommand("LIGHT", "WARM_MAX", StripLight::hotMax());
        ok &= sendSetCommand("LIGHT", "BRIGHTNESS_STEP", StripLight::brightnessStep());
        ok &= sendSetCommand("LIGHT", "KELVIN_STEP", StripLight::kelvinStep());
        ok &= sendSetCommand("BUTTONS", "CLICK_MS", StripLight::buttonClickTime());
        ok &= sendSetCommand("BUTTONS", "LONG_MS", StripLight::buttonLongTime());
        ok &= sendSetCommand("BUTTONS", "REPEAT_MS", StripLight::buttonDuringTime());
        update();
        return ok;
    }

    const Snapshot &snapshot()
    {
        return current;
    }
}
