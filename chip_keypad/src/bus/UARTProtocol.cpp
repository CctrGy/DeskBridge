#include "bus/UARTProtocol.h"

#include <Arduino.h>
#include <stdlib.h>
#include <string.h>

#include "core/PeripheralState.h"
#include "input/ButtonControls.h"

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
        peripheralSerial.println("OK");
    }

    void replyErr(const char *reason)
    {
        peripheralSerial.print("ERR ");
        peripheralSerial.println(reason);
    }

    void replyInfo()
    {
        peripheralSerial.println("OK DEV=66 PROTO=TXT1 NAME=CHIP_KEYPAD");
    }

    void replyStrip()
    {
        peripheralSerial.print("OK EN=");
        peripheralSerial.print(State.stripEnabled ? 1 : 0);
        peripheralSerial.print(" MODE=");
        peripheralSerial.print(static_cast<uint8_t>(State.stripMode));
        peripheralSerial.print(" VAL=");
        peripheralSerial.print(State.brightness);
        peripheralSerial.print(" KEL=");
        peripheralSerial.print(State.kelvin);
        peripheralSerial.print(" CMIN=");
        peripheralSerial.print(State.coldMin);
        peripheralSerial.print(" CMAX=");
        peripheralSerial.print(State.coldMax);
        peripheralSerial.print(" WMIN=");
        peripheralSerial.print(State.warmMin);
        peripheralSerial.print(" WMAX=");
        peripheralSerial.print(State.warmMax);
        peripheralSerial.print(" BSTEP=");
        peripheralSerial.print(State.brightnessStep);
        peripheralSerial.print(" KSTEP=");
        peripheralSerial.print(State.kelvinStep);
        peripheralSerial.print(" CT=");
        peripheralSerial.print(State.clickMs);
        peripheralSerial.print(" LT=");
        peripheralSerial.print(State.longMs);
        peripheralSerial.print(" RT=");
        peripheralSerial.print(State.repeatMs);
        peripheralSerial.print(" PWM_C=");
        peripheralSerial.print(State.pwmCold);
        peripheralSerial.print(" PWM_W=");
        peripheralSerial.println(State.pwmWarm);
    }

    void replyPower()
    {
        const uint32_t powerMw = (static_cast<uint32_t>(abs(State.currentMa)) * State.supplyMv) / 1000UL;

        peripheralSerial.print("OK MA=");
        peripheralSerial.print(State.currentMa);
        peripheralSerial.print(" RAW=");
        peripheralSerial.print(State.currentRawAdc);
        peripheralSerial.print(" MV=");
        peripheralSerial.print(State.supplyMv);
        peripheralSerial.print(" VRAW=");
        peripheralSerial.print(State.voltageRawAdc);
        peripheralSerial.print(" MW=");
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

        peripheralSerial.print("OK EVT=");
        peripheralSerial.print(events);
        peripheralSerial.print(" BP=");
        peripheralSerial.print(pressedMask);
        peripheralSerial.print(" BR=");
        peripheralSerial.print(releasedMask);
        peripheralSerial.print(" BD=");
        peripheralSerial.print(downMask);
        peripheralSerial.print(" LB=");
        peripheralSerial.print(lastButton);
        peripheralSerial.print(" BE=");
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

        peripheralSerial.print("OK BP=");
        peripheralSerial.print(State.pendingButtonPressedMask);
        peripheralSerial.print(" BR=");
        peripheralSerial.print(State.pendingButtonReleasedMask);
        peripheralSerial.print(" BD=");
        peripheralSerial.print(downMask);
        peripheralSerial.print(" LB=");
        peripheralSerial.print(State.lastButtonEventIndex);
        peripheralSerial.print(" BE=");
        peripheralSerial.println(static_cast<uint8_t>(State.lastButtonEventEdge));
        clearButtonEdges();
    }

    void replyAction()
    {
        char actionHex[9] = {};
        if (consumeCoreAction(actionHex, sizeof(actionHex)))
        {
            peripheralSerial.print("OK ACT=");
            peripheralSerial.println(actionHex);
            return;
        }

        peripheralSerial.println("OK ACT=NONE");
    }

    void replyButtonActionList()
    {
        peripheralSerial.print("OK");
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

        peripheralSerial.print(" N=");
        peripheralSerial.println(available);
    }

    void replyButtonAssignments()
    {
        peripheralSerial.print("OK");
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

    bool startsWith(const char *text, const char *prefix)
    {
        return strncmp(text, prefix, strlen(prefix)) == 0;
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

        if (strcmp(line, "PING") == 0)
        {
            peripheralSerial.println("OK PONG");
            return;
        }

        if (strcmp(line, "INF?") == 0)
        {
            replyInfo();
            return;
        }

        if (strcmp(line, "STR?") == 0)
        {
            replyStrip();
            return;
        }

        if (strcmp(line, "PWR?") == 0)
        {
            replyPower();
            return;
        }

        if (strcmp(line, "EVT?") == 0)
        {
            replyEvents();
            return;
        }

        if (strcmp(line, "BTN?") == 0)
        {
            replyButtonEdges();
            return;
        }

        if (strcmp(line, "ACT?") == 0)
        {
            replyAction();
            return;
        }

        if (strcmp(line, "BAL?") == 0)
        {
            replyButtonActionList();
            return;
        }

        if (strcmp(line, "BAS?") == 0)
        {
            replyButtonAssignments();
            return;
        }

        if (startsWith(line, "BSA"))
        {
            uint8_t buttonIndex = 0;
            ButtonTaskOption option = ButtonTaskOption::STRIP_POWER_AND_BRIGHTNESS;
            if (!parseButtonAssignment(line + 3, buttonIndex, option))
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

        if (strcmp(line, "SCL") == 0)
        {
            clearEvents();
            replyOk();
            return;
        }

        if (strcmp(line, "SRD") == 0)
        {
            resetStateDefaults();
            replyOk();
            return;
        }

        if (startsWith(line, "SSE"))
        {
            State.stripEnabled = strtoul(line + 3, nullptr, 10) != 0;
            replyOk();
            return;
        }

        if (startsWith(line, "SSM"))
        {
            const uint8_t mode = static_cast<uint8_t>(strtoul(line + 3, nullptr, 10));
            State.stripMode = mode <= static_cast<uint8_t>(StripMode::DualWhite) ? static_cast<StripMode>(mode) : StripMode::DualWhite;
            replyOk();
            return;
        }

        if (startsWith(line, "SSV"))
        {
            State.brightness = readU16Tail(line, 3);
            normalizeState();
            replyOk();
            return;
        }

        if (startsWith(line, "SSK"))
        {
            State.kelvin = readU16Tail(line, 3);
            normalizeState();
            replyOk();
            return;
        }

        if (startsWith(line, "SCN"))
        {
            State.coldMin = readU16Tail(line, 3);
            normalizeState();
            replyOk();
            return;
        }

        if (startsWith(line, "SCX"))
        {
            State.coldMax = readU16Tail(line, 3);
            normalizeState();
            replyOk();
            return;
        }

        if (startsWith(line, "SWN"))
        {
            State.warmMin = readU16Tail(line, 3);
            normalizeState();
            replyOk();
            return;
        }

        if (startsWith(line, "SWX"))
        {
            State.warmMax = readU16Tail(line, 3);
            normalizeState();
            replyOk();
            return;
        }

        if (startsWith(line, "SBS"))
        {
            State.brightnessStep = readU16Tail(line, 3);
            normalizeState();
            replyOk();
            return;
        }

        if (startsWith(line, "SKS"))
        {
            State.kelvinStep = readU16Tail(line, 3);
            normalizeState();
            replyOk();
            return;
        }

        if (startsWith(line, "SCT"))
        {
            State.clickMs = readMsTail(line, 3);
            normalizeState();
            replyOk();
            return;
        }

        if (startsWith(line, "SLT"))
        {
            State.longMs = readMsTail(line, 3);
            normalizeState();
            replyOk();
            return;
        }

        if (startsWith(line, "SRT"))
        {
            State.repeatMs = readMsTail(line, 3);
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
