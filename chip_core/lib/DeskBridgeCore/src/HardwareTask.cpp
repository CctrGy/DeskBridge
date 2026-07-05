#include "HardwareTask.hpp"

#include "core/DeskBridgeVersion.h"
#include "core/ErrorCodes.h"
#include "core/DeskHardware.h"
#include "bus/KeypadPeripheral.h"
#include "light/NotificationPixels.h"
#include "USB.hpp"
#include "libs.h"

namespace
{
    Shell controlShell;

    constexpr uint16_t HID_CONSUMER_SCAN_NEXT = 0x00B5;
    constexpr uint16_t HID_CONSUMER_SCAN_PREVIOUS = 0x00B6;
    constexpr uint16_t HID_CONSUMER_PLAY_PAUSE = 0x00CD;
    constexpr uint16_t HID_CONSUMER_MUTE = 0x00E2;
    constexpr uint16_t HID_CONSUMER_VOLUME_INCREMENT = 0x00E9;
    constexpr uint16_t HID_CONSUMER_VOLUME_DECREMENT = 0x00EA;
    constexpr uint8_t HID_KEY_F13 = 0x68;
    constexpr uint8_t HID_KEY_F14 = 0x69;
    constexpr uint8_t HID_KEY_F15 = 0x6A;

    uint32_t shellRead(void *buffer, uint32_t length)
    {
        return DeskBridgeUSB.getCom().read(buffer, length);
    }

    uint32_t shellWrite(const void *buffer, uint32_t length)
    {
        const uint32_t written = DeskBridgeUSB.getCom().write(buffer, length);
        DeskUSB::flush(DeskUSB::CONTROL);
        return written;
    }

    const char *keypadEventName(uint8_t eventCode)
    {
        switch (eventCode)
        {
            case 1:
                return "pressed";
            case 2:
                return "short_released";
            case 3:
                return "long_released";
            default:
                return "unknown";
        }
    }

    void tapKeyboard(uint8_t keycode)
    {
        DeskBridgeUSB.getKeyboard().press(0, keycode);
        DeskBridgeUSB.task();
        DeskBridgeUSB.getKeyboard().release();
    }

    void executeKeypadAction(const KeypadPeripheral::Snapshot &keypad)
    {
        if (keypad.lastButtonEventCode == 1 || keypad.lastActionId == 0)
        {
            return;
        }

        switch (keypad.lastActionId)
        {
            case 1:
                DeskBridgeUSB.getKeyboard().media(HID_CONSUMER_MUTE);
                break;
            case 2:
                DeskBridgeUSB.getKeyboard().media(HID_CONSUMER_VOLUME_INCREMENT);
                break;
            case 3:
                DeskBridgeUSB.getKeyboard().media(HID_CONSUMER_VOLUME_DECREMENT);
                break;
            case 4:
                DeskBridgeUSB.getKeyboard().media(HID_CONSUMER_PLAY_PAUSE);
                break;
            case 5:
                DeskBridgeUSB.getKeyboard().media(HID_CONSUMER_SCAN_NEXT);
                break;
            case 6:
                DeskBridgeUSB.getKeyboard().media(HID_CONSUMER_SCAN_PREVIOUS);
                break;
            case 7:
                tapKeyboard(HID_KEY_F13);
                break;
            case 8:
                tapKeyboard(HID_KEY_F14);
                break;
            case 9:
                tapKeyboard(HID_KEY_F15);
                break;
            default:
                break;
        }
    }
}

void HardwareTask::begin()
{
    DeskBridgeUSB.begin();
    controlShell.begin(shellRead, shellWrite);
    DeskHardware::begin();
    heartbeat_.reset();
}

void HardwareTask::updateBackend()
{
    loopStartUs_ = micros();
    ++cycles_;

    DeskBridgeUSB.task();
    controlShell.update();
    DeskHardware::updateBackend();
    updateUsbNotifications();

    if (DeskBridgeUSB.getCom().connected() && KeypadPeripheral::consumeButtonEventNotification())
    {
        const KeypadPeripheral::Snapshot &keypad = KeypadPeripheral::snapshot();
        NotificationPixels::showKeypadEvent(keypad.lastButton, keypad.lastButtonEventCode);
        executeKeypadAction(keypad);

        char eventLine[192] = {};
        snprintf(eventLine, sizeof(eventLine), "[EVT] keypad.button button:%u event:%s edge:%u ec:%u pressed:%u released:%u down:%u aid:%u action:%s",
                 keypad.lastButton,
                 keypadEventName(keypad.lastButtonEventCode),
                 keypad.lastButtonEdge,
                 keypad.lastButtonEventCode,
                 keypad.buttonPressedMask,
                 keypad.buttonReleasedMask,
                 keypad.buttonDownMask,
                 keypad.lastActionId,
                 keypad.lastAction[0] != '\0' ? keypad.lastAction : "none");
        controlShell.writeAsyncLine(eventLine);

        char notifyLine[160] = {};
        snprintf(notifyLine, sizeof(notifyLine), "button:%u event:%s action:%s",
                 keypad.lastButton,
                 keypadEventName(keypad.lastButtonEventCode),
                 keypad.lastAction[0] != '\0' ? keypad.lastAction : "none");
        controlShell.writeAsyncTuiNotification("0x00042001", "keypad event", notifyLine);

        char dataLine[128] = {};
        snprintf(dataLine, sizeof(dataLine), "[DAT] keypad: events:%u, down:%u, last:%u, event_code:%u, action_id:%u, irq_count:%lu",
                 keypad.events,
                 keypad.buttonDownMask,
                 keypad.lastButton,
                 keypad.lastButtonEventCode,
                 keypad.lastActionId,
                 static_cast<unsigned long>(keypad.eventInterruptCount));
        controlShell.writeAsyncLine(dataLine);
    }

    if (!identityLogged_ && DeskBridgeUSB.getCom().connected())
    {
        logFirmwareIdentity();
        identityLogged_ = true;
    }

    logHeartbeat();

    lastLoopMicros_ = micros() - loopStartUs_;
    if (lastLoopMicros_ > maxLoopMicros_)
    {
        maxLoopMicros_ = lastLoopMicros_;
    }
}

void HardwareTask::updateInterface()
{
    DeskHardware::updateInterface();
}

void HardwareTask::update()
{
    updateBackend();
    updateInterface();
}

void HardwareTask::logFirmwareIdentity()
{
    DeskBridgeVersion::Info version = DeskBridgeVersion::current();

    DeskBridgeUSB.log("[LOG] firmware: ");
    DeskBridgeUSB.log(version.firmware);
    DeskBridgeUSB.log(", serial_protocol:0x");

    char protocolHex[] = {
        "0123456789ABCDEF"[version.serialProtocol >> 4],
        "0123456789ABCDEF"[version.serialProtocol & 0x0F],
        '\0',
    };
    DeskBridgeUSB.log(protocolHex);
    DeskBridgeUSB.logln("");
}

void HardwareTask::updateUsbNotifications()
{
    if (!DeskBridgeUSB.getCom().connected())
    {
        keypadPresenceKnown_ = false;
        return;
    }

    const bool keypadPresent = KeypadPeripheral::present();
    if (keypadPresenceKnown_ && keypadPresent == lastKeypadPresent_)
    {
        return;
    }

    keypadPresenceKnown_ = true;
    lastKeypadPresent_ = keypadPresent;

    char code[12] = {};
    if (!keypadPresent)
    {
        snprintf(code, sizeof(code), "0x%08lX", static_cast<unsigned long>(DeskError::KeypadMissing));
        controlShell.writeAsyncTuiNotification(code, "keypad missing", "CHIP_KEYPAD no responde por UART");
        return;
    }

    controlShell.writeAsyncTuiNotification("0x00040000", "keypad restored", "CHIP_KEYPAD vuelve a responder por UART");
}

void HardwareTask::logHeartbeat()
{
    if (heartbeat_.elapsed())
    {
        // Keep the control CDC clean for request/response shell commands.
    }
}
