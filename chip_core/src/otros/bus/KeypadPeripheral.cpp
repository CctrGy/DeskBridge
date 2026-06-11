#include "bus/KeypadPeripheral.h"

#include <Arduino.h>
#include <string.h>

#include <KeypadProtocol.h>

#include "config/DeskConfig.h"
#include "config/pins_config.h"
#include "light/StripLight.h"

namespace Protocol = DeskBridgeKeypadProtocol;

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
        Protocol::clearInput(keypadSerial);
#endif
    }

    bool readLine(char *buffer, uint8_t capacity)
    {
#if !KEYPAD_UART_CONFIGURED
        (void)buffer;
        (void)capacity;
        return false;
#else
        return Protocol::readLine(keypadSerial, buffer, capacity, KEYPAD_UART_TIMEOUT_MS_DEFAULT);
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
        return Protocol::sendLine(keypadSerial, line, response, responseCapacity, KEYPAD_UART_TIMEOUT_MS_DEFAULT);
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
        Protocol::formatValueCommand(command, sizeof(command), prefix, value);
        return sendCommand(command);
    }

    bool sendCommandBool(const char *prefix, bool value)
    {
        char command[20] = {};
        Protocol::formatBoolCommand(command, sizeof(command), prefix, value);
        return sendCommand(command);
    }

    void parseStripResponse(const char *response)
    {
        long numeric = 0;
        bool enabled = false;

        if (Protocol::boolForKey(response, Protocol::Field::Enabled, enabled))
        {
            current.stripEnabled = enabled;
        }
        if (Protocol::valueForKey(response, Protocol::Field::Mode, numeric))
        {
            current.stripMode = static_cast<uint8_t>(numeric);
        }
        if (Protocol::valueForKey(response, Protocol::Field::Brightness, numeric))
        {
            current.brightness = static_cast<uint16_t>(numeric);
        }
        if (Protocol::valueForKey(response, Protocol::Field::Kelvin, numeric))
        {
            current.kelvin = static_cast<uint16_t>(numeric);
        }
        if (Protocol::valueForKey(response, Protocol::Field::ColdMin, numeric))
        {
            current.coldMin = static_cast<uint16_t>(numeric);
        }
        if (Protocol::valueForKey(response, Protocol::Field::ColdMax, numeric))
        {
            current.coldMax = static_cast<uint16_t>(numeric);
        }
        if (Protocol::valueForKey(response, Protocol::Field::WarmMin, numeric))
        {
            current.warmMin = static_cast<uint16_t>(numeric);
        }
        if (Protocol::valueForKey(response, Protocol::Field::WarmMax, numeric))
        {
            current.warmMax = static_cast<uint16_t>(numeric);
        }
        if (Protocol::valueForKey(response, Protocol::Field::BrightnessStep, numeric))
        {
            current.brightnessStep = static_cast<uint16_t>(numeric);
        }
        if (Protocol::valueForKey(response, Protocol::Field::KelvinStep, numeric))
        {
            current.kelvinStep = static_cast<uint16_t>(numeric);
        }
        if (Protocol::valueForKey(response, Protocol::Field::ClickMs, numeric))
        {
            current.clickMs = static_cast<uint16_t>(numeric);
        }
        if (Protocol::valueForKey(response, Protocol::Field::LongMs, numeric))
        {
            current.longMs = static_cast<uint16_t>(numeric);
        }
        if (Protocol::valueForKey(response, Protocol::Field::RepeatMs, numeric))
        {
            current.repeatMs = static_cast<uint16_t>(numeric);
        }
        if (Protocol::valueForKey(response, Protocol::Field::PwmCold, numeric))
        {
            current.pwmCold = static_cast<uint16_t>(numeric);
        }
        if (Protocol::valueForKey(response, Protocol::Field::PwmWarm, numeric))
        {
            current.pwmWarm = static_cast<uint16_t>(numeric);
        }
    }

    void parsePowerResponse(const char *response)
    {
        long numeric = 0;

        if (Protocol::valueForKey(response, Protocol::Field::CurrentMa, numeric))
        {
            current.currentMa = static_cast<int16_t>(numeric);
        }
        if (Protocol::valueForKey(response, Protocol::Field::CurrentRaw, numeric))
        {
            current.currentRaw = static_cast<uint16_t>(numeric);
        }
        if (Protocol::valueForKey(response, Protocol::Field::SupplyMv, numeric))
        {
            current.supplyMv = static_cast<uint16_t>(numeric);
        }
        if (Protocol::valueForKey(response, Protocol::Field::VoltageRaw, numeric))
        {
            current.voltageRaw = static_cast<uint16_t>(numeric);
        }
        if (Protocol::valueForKey(response, Protocol::Field::PowerMw, numeric))
        {
            current.powerMw = static_cast<uint32_t>(numeric);
        }
    }

    void parseInfoResponse(const char *response)
    {
        Protocol::stringForKey(response, Protocol::Field::Protocol, current.protocolText, sizeof(current.protocolText));
    }

    void parseEventResponse(const char *response)
    {
        long numeric = 0;

        if (Protocol::valueForKey(response, Protocol::Field::Events, numeric))
        {
            current.events = static_cast<uint16_t>(numeric);
        }
        if (Protocol::valueForKey(response, Protocol::Field::ButtonPressed, numeric))
        {
            current.buttonPressedMask = static_cast<uint8_t>(numeric);
        }
        if (Protocol::valueForKey(response, Protocol::Field::ButtonReleased, numeric))
        {
            current.buttonReleasedMask = static_cast<uint8_t>(numeric);
        }
        if (Protocol::valueForKey(response, Protocol::Field::ButtonDown, numeric))
        {
            current.buttonDownMask = static_cast<uint8_t>(numeric);
        }
        if (Protocol::valueForKey(response, Protocol::Field::LastButton, numeric))
        {
            current.lastButton = static_cast<uint8_t>(numeric);
        }
        if (Protocol::valueForKey(response, Protocol::Field::ButtonEdge, numeric))
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
        if (Protocol::stringForKey(response, Protocol::Field::Action, action, sizeof(action)) &&
            strcmp(action, Protocol::Response::ActionNone) != 0)
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
            if (Protocol::valueForKey(response, key, numeric))
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
        current.present = sendLine(Protocol::Command::Ping, response, sizeof(response));
        if (!current.present)
        {
            current.protocol = 0;
            return;
        }

        current.protocol = Protocol::ProtocolVersion;
        current.statePin = digitalRead(KEYPAD_STATE_PIN) == HIGH;

        if (sendLine(Protocol::Command::Info, response, sizeof(response)))
        {
            parseInfoResponse(response);
        }

        serviceEvents();

        if (sendLine(Protocol::Command::Strip, response, sizeof(response)))
        {
            parseStripResponse(response);
        }

        if (sendLine(Protocol::Command::Power, response, sizeof(response)))
        {
            parsePowerResponse(response);
        }

        if (sendLine(Protocol::Command::ButtonAssignments, response, sizeof(response)))
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
        if (!sendLine(Protocol::Command::Events, response, sizeof(response)))
        {
            current.present = false;
            return;
        }

        current.present = true;
        parseEventResponse(response);

        if ((current.events & EventCoreCommand) != 0)
        {
            if (sendLine(Protocol::Command::Action, response, sizeof(response)))
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
        sendCommand(Protocol::Command::ClearEvents);
        current.events = 0;
        current.status &= ~0x02;
    }

    bool command(uint8_t commandValue)
    {
        switch (commandValue)
        {
            case 1:
                return sendCommand(Protocol::Command::ClearEvents);
            case 2:
                return sendCommand(Protocol::Command::ResetDefaults);
            case 3:
                return sendCommandBool(Protocol::Prefix::SetEnabled, !current.stripEnabled);
            case 4:
                return sendCommandValue(Protocol::Prefix::SetKelvin, 0);
            case 5:
                return sendCommandValue(Protocol::Prefix::SetKelvin, StripLight::pwmMax() / 2);
            case 6:
                return sendCommandValue(Protocol::Prefix::SetKelvin, StripLight::pwmMax());
            default:
                return false;
        }
    }

    bool writeU16(uint8_t regLow, uint16_t value)
    {
        switch (regLow)
        {
            case 0x08:
                return sendCommandValue(Protocol::Prefix::SetBrightness, value);
            case 0x0A:
                return sendCommandValue(Protocol::Prefix::SetKelvin, value);
            case 0x0C:
                return sendCommandValue(Protocol::Prefix::SetColdMin, value);
            case 0x0E:
                return sendCommandValue(Protocol::Prefix::SetColdMax, value);
            case 0x10:
                return sendCommandValue(Protocol::Prefix::SetWarmMin, value);
            case 0x12:
                return sendCommandValue(Protocol::Prefix::SetWarmMax, value);
            case 0x14:
                return sendCommandValue(Protocol::Prefix::SetBrightnessStep, value);
            case 0x16:
                return sendCommandValue(Protocol::Prefix::SetKelvinStep, value);
            case 0x1A:
                return sendCommandValue(Protocol::Prefix::SetClickMs, value);
            case 0x1C:
                return sendCommandValue(Protocol::Prefix::SetLongMs, value);
            case 0x1E:
                return sendCommandValue(Protocol::Prefix::SetRepeatMs, value);
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
        Protocol::formatButtonAssignmentCommand(commandBuffer, sizeof(commandBuffer), buttonIndex, actionId);
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
        ok &= sendCommandBool(Protocol::Prefix::SetEnabled, StripLight::enabled());
        ok &= sendCommandValue(Protocol::Prefix::SetMode, stripModeForKeypad());
        ok &= sendCommandValue(Protocol::Prefix::SetBrightness, StripLight::brightness());
        ok &= sendCommandValue(Protocol::Prefix::SetKelvin, StripLight::kelvinMix());
        ok &= sendCommandValue(Protocol::Prefix::SetColdMin, StripLight::coldMin());
        ok &= sendCommandValue(Protocol::Prefix::SetColdMax, StripLight::coldMax());
        ok &= sendCommandValue(Protocol::Prefix::SetWarmMin, StripLight::hotMin());
        ok &= sendCommandValue(Protocol::Prefix::SetWarmMax, StripLight::hotMax());
        ok &= sendCommandValue(Protocol::Prefix::SetBrightnessStep, StripLight::brightnessStep());
        ok &= sendCommandValue(Protocol::Prefix::SetKelvinStep, StripLight::kelvinStep());
        ok &= sendCommandValue(Protocol::Prefix::SetClickMs, StripLight::buttonClickTime());
        ok &= sendCommandValue(Protocol::Prefix::SetLongMs, StripLight::buttonLongTime());
        ok &= sendCommandValue(Protocol::Prefix::SetRepeatMs, StripLight::buttonDuringTime());
        update();
        return ok;
    }

    const Snapshot &snapshot()
    {
        return current;
    }
}
