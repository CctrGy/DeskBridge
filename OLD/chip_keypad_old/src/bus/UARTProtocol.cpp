#include "bus/UARTProtocol.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

#include <KeypadProtocol.h>

#include "core/PeripheralState.h"
#include "input/ButtonControls.h"

namespace Protocol = DeskBridgeKeypadProtocol;

namespace
{
    HardwareSerial peripheralSerial(PeripheralConfig::UART_RX_PIN, PeripheralConfig::UART_TX_PIN);

    constexpr uint8_t lineCapacity = 48;
    char lineBuffer[lineCapacity] = {};
    uint8_t lineLength = 0;

    uint16_t readU16Tail(const char *text, uint8_t offset)
    {
        return static_cast<uint16_t>(constrain(strtoul(text + offset, nullptr, 10), 0UL, static_cast<unsigned long>(PeripheralConfig::PWM_MAX)));
    }

    uint16_t readMsTail(const char *text, uint8_t offset)
    {
        return static_cast<uint16_t>(constrain(strtoul(text + offset, nullptr, 10), 1UL, 60000UL));
    }

    void replyOk()
    {
        peripheralSerial.println(Protocol::Response::Ok);
    }

    void replyErr(const char *reason)
    {
        peripheralSerial.print(Protocol::Response::Err);
        peripheralSerial.print(' ');
        peripheralSerial.println(reason);
    }

    void replyInfo()
    {
        peripheralSerial.print(Protocol::Response::Ok);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::Device);
        peripheralSerial.print(Protocol::DeviceId);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::Protocol);
        peripheralSerial.print(Protocol::ProtocolText);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::Name);
        peripheralSerial.println("CHIP_KEYPAD");
    }

    void replyStrip()
    {
        peripheralSerial.print(Protocol::Response::Ok);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::Enabled);
        peripheralSerial.print(State.stripEnabled ? 1 : 0);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::Mode);
        peripheralSerial.print(static_cast<uint8_t>(State.stripMode));
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::Brightness);
        peripheralSerial.print(State.brightness);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::Kelvin);
        peripheralSerial.print(State.kelvin);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::ColdMin);
        peripheralSerial.print(State.coldMin);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::ColdMax);
        peripheralSerial.print(State.coldMax);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::WarmMin);
        peripheralSerial.print(State.warmMin);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::WarmMax);
        peripheralSerial.print(State.warmMax);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::BrightnessStep);
        peripheralSerial.print(State.brightnessStep);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::KelvinStep);
        peripheralSerial.print(State.kelvinStep);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::ClickMs);
        peripheralSerial.print(State.clickMs);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::LongMs);
        peripheralSerial.print(State.longMs);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::RepeatMs);
        peripheralSerial.print(State.repeatMs);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::PwmCold);
        peripheralSerial.print(State.pwmCold);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::PwmWarm);
        peripheralSerial.println(State.pwmWarm);
    }

    void replyPower()
    {
        const uint32_t powerMw = (static_cast<uint32_t>(abs(State.currentMa)) * State.supplyMv) / 1000UL;

        peripheralSerial.print(Protocol::Response::Ok);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::CurrentMa);
        peripheralSerial.print(State.currentMa);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::CurrentRaw);
        peripheralSerial.print(State.currentRawAdc);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::SupplyMv);
        peripheralSerial.print(State.supplyMv);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::VoltageRaw);
        peripheralSerial.print(State.voltageRawAdc);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::PowerMw);
        peripheralSerial.println(powerMw);
    }

    void replyEvents()
    {
        const uint16_t events = readEvents();
        const uint8_t pressedMask = State.pendingButtonPressedMask;
        const uint8_t releasedMask = State.pendingButtonReleasedMask;
        const uint8_t lastButton = State.lastButtonEventIndex;
        const ButtonEdge lastEdge = State.lastButtonEventEdge;
        uint8_t downMask = 0;
        for (uint8_t index = 0; index < PeripheralConfig::BUTTON_COUNT; ++index)
        {
            if (State.buttonDown[index])
            {
                downMask |= 1 << index;
            }
        }

        peripheralSerial.print(Protocol::Response::Ok);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::Events);
        peripheralSerial.print(events);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::ButtonPressed);
        peripheralSerial.print(pressedMask);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::ButtonReleased);
        peripheralSerial.print(releasedMask);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::ButtonDown);
        peripheralSerial.print(downMask);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::LastButton);
        peripheralSerial.print(lastButton);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::ButtonEdge);
        peripheralSerial.println(static_cast<uint8_t>(lastEdge));
        clearEvents();
        clearButtonEdges();
    }

    void replyButtonEdges()
    {
        uint8_t downMask = 0;
        for (uint8_t index = 0; index < PeripheralConfig::BUTTON_COUNT; ++index)
        {
            if (State.buttonDown[index])
            {
                downMask |= 1 << index;
            }
        }

        peripheralSerial.print(Protocol::Response::Ok);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::ButtonPressed);
        peripheralSerial.print(State.pendingButtonPressedMask);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::ButtonReleased);
        peripheralSerial.print(State.pendingButtonReleasedMask);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::ButtonDown);
        peripheralSerial.print(downMask);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::LastButton);
        peripheralSerial.print(State.lastButtonEventIndex);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::ButtonEdge);
        peripheralSerial.println(static_cast<uint8_t>(State.lastButtonEventEdge));
        clearButtonEdges();
    }

    void replyAction()
    {
        char actionHex[9] = {};
        if (consumeCoreAction(actionHex, sizeof(actionHex)))
        {
            peripheralSerial.print(Protocol::Response::Ok);
            peripheralSerial.print(' ');
            peripheralSerial.print(Protocol::Field::Action);
            peripheralSerial.println(actionHex);
            return;
        }

        peripheralSerial.print(Protocol::Response::Ok);
        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::Action);
        peripheralSerial.println(Protocol::Response::ActionNone);
    }

    void replyButtonActionList()
    {
        peripheralSerial.print(Protocol::Response::Ok);
        const ButtonTaskDefinition *definitions = ButtonControls::taskDefinitions();
        const uint8_t count = ButtonControls::taskDefinitionCount();
        uint8_t available = 0;

        for (uint8_t index = 0; index < count; ++index)
        {
            if (ButtonControls::taskAvailable(definitions[index].option))
            {
                peripheralSerial.print(" A");
                peripheralSerial.print(available++);
                peripheralSerial.print("=");
                peripheralSerial.print(static_cast<uint8_t>(definitions[index].option));
                peripheralSerial.print(":");
                peripheralSerial.print(definitions[index].name);
            }
        }

        peripheralSerial.print(' ');
        peripheralSerial.print(Protocol::Field::AvailableCount);
        peripheralSerial.println(available);
    }

    void replyButtonAssignments()
    {
        peripheralSerial.print(Protocol::Response::Ok);
        for (uint8_t index = 0; index < PeripheralConfig::BUTTON_COUNT; ++index)
        {
            peripheralSerial.print(" B");
            peripheralSerial.print(index);
            peripheralSerial.print("=");
            peripheralSerial.print(static_cast<uint8_t>(ButtonControls::buttonTask(index)));
        }
        peripheralSerial.println();
    }

    bool parseButtonAssignment(const char *text, uint8_t &buttonIndex, ButtonTaskOption &option)
    {
        char *separator = strchr(const_cast<char *>(text), ',');
        if (separator == nullptr)
        {
            return false;
        }

        *separator = '\0';
        buttonIndex = static_cast<uint8_t>(strtoul(text, nullptr, 10));
        const uint8_t rawOption = static_cast<uint8_t>(strtoul(separator + 1, nullptr, 10));
        if (rawOption >= BUTTON_TASK_OPTION_COUNT)
        {
            return false;
        }

        option = static_cast<ButtonTaskOption>(rawOption);
        return true;
    }

    void executeLine(char *line)
    {
        while (*line == ' ')
        {
            ++line;
        }

        if (*line == '\0')
        {
            return;
        }

        if (strcmp(line, Protocol::Command::Ping) == 0)
        {
            peripheralSerial.println(Protocol::Response::Pong);
            return;
        }

        if (strcmp(line, Protocol::Command::Info) == 0)
        {
            replyInfo();
            return;
        }

        if (strcmp(line, Protocol::Command::Strip) == 0)
        {
            replyStrip();
            return;
        }

        if (strcmp(line, Protocol::Command::Power) == 0)
        {
            replyPower();
            return;
        }

        if (strcmp(line, Protocol::Command::Events) == 0)
        {
            replyEvents();
            return;
        }

        if (strcmp(line, Protocol::Command::Buttons) == 0)
        {
            replyButtonEdges();
            return;
        }

        if (strcmp(line, Protocol::Command::Action) == 0)
        {
            replyAction();
            return;
        }

        if (strcmp(line, Protocol::Command::ButtonActionList) == 0)
        {
            replyButtonActionList();
            return;
        }

        if (strcmp(line, Protocol::Command::ButtonAssignments) == 0)
        {
            replyButtonAssignments();
            return;
        }

        if (Protocol::startsWith(line, Protocol::Prefix::SetButtonAction))
        {
            uint8_t buttonIndex = 0;
            ButtonTaskOption option = ButtonTaskOption::STRIP_POWER_AND_BRIGHTNESS;
            if (!parseButtonAssignment(line + strlen(Protocol::Prefix::SetButtonAction), buttonIndex, option))
            {
                replyErr("BAD_ARGS");
                return;
            }

            if (!ButtonControls::setButtonTask(buttonIndex, option))
            {
                replyErr("BAD_TASK");
                return;
            }

            replyOk();
            return;
        }

        if (strcmp(line, Protocol::Command::ClearEvents) == 0)
        {
            clearEvents();
            replyOk();
            return;
        }

        if (strcmp(line, Protocol::Command::ResetDefaults) == 0)
        {
            resetStateDefaults();
            replyOk();
            return;
        }

        if (Protocol::startsWith(line, Protocol::Prefix::SetEnabled))
        {
            State.stripEnabled = strtoul(line + strlen(Protocol::Prefix::SetEnabled), nullptr, 10) != 0;
            replyOk();
            return;
        }

        if (Protocol::startsWith(line, Protocol::Prefix::SetMode))
        {
            const uint8_t mode = static_cast<uint8_t>(strtoul(line + strlen(Protocol::Prefix::SetMode), nullptr, 10));
            State.stripMode = mode <= static_cast<uint8_t>(StripMode::DualWhite) ? static_cast<StripMode>(mode) : StripMode::DualWhite;
            replyOk();
            return;
        }

        if (Protocol::startsWith(line, Protocol::Prefix::SetBrightness))
        {
            State.brightness = readU16Tail(line, strlen(Protocol::Prefix::SetBrightness));
            normalizeState();
            replyOk();
            return;
        }

        if (Protocol::startsWith(line, Protocol::Prefix::SetKelvin))
        {
            State.kelvin = readU16Tail(line, strlen(Protocol::Prefix::SetKelvin));
            normalizeState();
            replyOk();
            return;
        }

        if (Protocol::startsWith(line, Protocol::Prefix::SetColdMin))
        {
            State.coldMin = readU16Tail(line, strlen(Protocol::Prefix::SetColdMin));
            normalizeState();
            replyOk();
            return;
        }

        if (Protocol::startsWith(line, Protocol::Prefix::SetColdMax))
        {
            State.coldMax = readU16Tail(line, strlen(Protocol::Prefix::SetColdMax));
            normalizeState();
            replyOk();
            return;
        }

        if (Protocol::startsWith(line, Protocol::Prefix::SetWarmMin))
        {
            State.warmMin = readU16Tail(line, strlen(Protocol::Prefix::SetWarmMin));
            normalizeState();
            replyOk();
            return;
        }

        if (Protocol::startsWith(line, Protocol::Prefix::SetWarmMax))
        {
            State.warmMax = readU16Tail(line, strlen(Protocol::Prefix::SetWarmMax));
            normalizeState();
            replyOk();
            return;
        }

        if (Protocol::startsWith(line, Protocol::Prefix::SetBrightnessStep))
        {
            State.brightnessStep = readU16Tail(line, strlen(Protocol::Prefix::SetBrightnessStep));
            normalizeState();
            replyOk();
            return;
        }

        if (Protocol::startsWith(line, Protocol::Prefix::SetKelvinStep))
        {
            State.kelvinStep = readU16Tail(line, strlen(Protocol::Prefix::SetKelvinStep));
            normalizeState();
            replyOk();
            return;
        }

        if (Protocol::startsWith(line, Protocol::Prefix::SetClickMs))
        {
            State.clickMs = readMsTail(line, strlen(Protocol::Prefix::SetClickMs));
            normalizeState();
            replyOk();
            return;
        }

        if (Protocol::startsWith(line, Protocol::Prefix::SetLongMs))
        {
            State.longMs = readMsTail(line, strlen(Protocol::Prefix::SetLongMs));
            normalizeState();
            replyOk();
            return;
        }

        if (Protocol::startsWith(line, Protocol::Prefix::SetRepeatMs))
        {
            State.repeatMs = readMsTail(line, strlen(Protocol::Prefix::SetRepeatMs));
            normalizeState();
            replyOk();
            return;
        }

        replyErr("UNKNOWN");
    }

    void consumeByte(char value)
    {
        if (value == '\r')
        {
            return;
        }

        if (value == '\n')
        {
            lineBuffer[lineLength] = '\0';
            executeLine(lineBuffer);
            lineLength = 0;
            return;
        }

        if (lineLength + 1 < lineCapacity)
        {
            lineBuffer[lineLength++] = value;
        }
        else
        {
            lineLength = 0;
            replyErr("LINE_TOO_LONG");
        }
    }
}

namespace UARTProtocol
{
    void begin()
    {
        peripheralSerial.begin(PeripheralConfig::UART_BAUD);
        lineLength = 0;
    }

    void update()
    {
        while (peripheralSerial.available() > 0)
        {
            consumeByte(static_cast<char>(peripheralSerial.read()));
        }
    }
}
