#include "core/DeskHardware.h"

#include <Arduino.h>

#include "bus/DeskBus.h"
#include "bus/KeypadPeripheral.h"
#include "config/DeskConfig.h"
#include "config/pins_config.h"
#include "core/DeskSettings.h"
#include "core/McuMonitor.h"
#include "core/StatusIndicator.h"
#include "input/PanelControls.h"
#include "light/NotificationPixels.h"
#include "light/StripLight.h"
#include "ui/OledPanel.h"
#include "usb/DeskUSB.h"

namespace
{
    bool resetKeyWasPressed = false;
    bool resetKeyAlreadyHandled = false;
    uint32_t resetKeyPressedSinceMs = 0;
    bool keypadStatePixelKnown = false;
    bool lastKeypadStatePixel = false;

    void beginConfigResetKey()
    {
        pinMode(BUILTIN_KEY_PIN, INPUT_PULLDOWN);
    }

    void updateConfigResetKey()
    {
        const bool pressed = digitalRead(BUILTIN_KEY_PIN) == HIGH;
        const uint32_t now = millis();

        if (pressed && !resetKeyWasPressed)
        {
            resetKeyWasPressed = true;
            resetKeyAlreadyHandled = false;
            resetKeyPressedSinceMs = now;
        }
        else if (!pressed)
        {
            resetKeyWasPressed = false;
            resetKeyAlreadyHandled = false;
        }

        if (pressed && !resetKeyAlreadyHandled && now - resetKeyPressedSinceMs >= CONFIG_RESET_KEY_HOLD_MS_DEFAULT)
        {
            DeskSettings::resetDefaults();
            DeskSettings::saveNow();
            KeypadPeripheral::syncStripConfig();
            resetKeyAlreadyHandled = true;
        }
    }

    void updateKeypadButtonNotification()
    {
        KeypadPeripheral::serviceEvents();
        KeypadPeripheral::consumeButtonEventNotification();

        const bool active = KeypadPeripheral::stateActive();
        if (keypadStatePixelKnown && active == lastKeypadStatePixel)
        {
            return;
        }

        keypadStatePixelKnown = true;
        lastKeypadStatePixel = active;

        if (active)
        {
            NotificationPixels::set(NOTIFICATION_KEYPAD_BUTTON_PIXEL_DEFAULT,
                                    NOTIFICATION_KEYPAD_BUTTON_RED_DEFAULT,
                                    NOTIFICATION_KEYPAD_BUTTON_GREEN_DEFAULT,
                                    NOTIFICATION_KEYPAD_BUTTON_BLUE_DEFAULT);
            return;
        }

        NotificationPixels::set(NOTIFICATION_KEYPAD_BUTTON_PIXEL_DEFAULT, 0, 0, 0);
    }
}

namespace DeskHardware
{
    void begin()
    {
        beginConfigResetKey();
        StatusIndicator::begin();
        StripLight::begin();
        NotificationPixels::begin();
        PanelControls::begin();
        McuMonitor::begin();
        OledPanel::begin();
        OledPanel::bootBegin();

        OledPanel::showBootStatus(DeskUSB::mounted() ? "USB=OK" : "USB=WAIT");
        OledPanel::showBootStatus("SPI=OK");
        OledPanel::showBootStatus("OLED=OK");
        OledPanel::showBootStatus("NeoPixel=OK");

        DeskBus::begin();
        OledPanel::showBootStatus("I2C PB9/PB8");
        OledPanel::showBootStatus("KEY UART PB7/PB6");
        OledPanel::showBootStatus(DeskBus::ready(DeskBus::Device::DS3231) ? "DS3231=OK" : "DS3231=MISS");
        OledPanel::showBootStatus(DeskBus::ready(DeskBus::Device::AT24C32) ? "AT24C32=OK" : "AT24C32=MISS");
        OledPanel::showBootStatus(DeskBus::ready(DeskBus::Device::Keypad) ? "KEYPAD=OK" : "KEYPAD=MISS");
        OledPanel::showBootStatus(DeskBus::ready(DeskBus::Device::ENS160) ? "ENS160=OK" : "ENS160=MISS");
        OledPanel::showBootStatus(DeskBus::ready(DeskBus::Device::AHT21) ? "AHT21=OK" : "AHT21=MISS");
        OledPanel::showBootStatus(DeskBus::ready(DeskBus::Device::VEML7700) ? "VEML7700=OK" : "VEML7700=MISS");
        OledPanel::showBootStatus(DeskBus::ready(DeskBus::Device::SCD41) ? "SCD41=OK" : "SCD41=MISS");
        OledPanel::showBootStatus(DeskBus::ready(DeskBus::Device::SHT21) ? "SHT21=OK" : "SHT21=MISS");
        DeskSettings::begin();
        OledPanel::showBootStatus(DeskSettings::storageReady() ? "EEPROM CFG=OK" : "EEPROM CFG=RAM");
        OledPanel::bootFinish();
    }

    void update()
    {
        updateConfigResetKey();
        StripLight::update();
        PanelControls::update();
        updateKeypadButtonNotification();
        OledPanel::update();
        DeskBus::update();
        NotificationPixels::update();
        DeskSettings::update();
        McuMonitor::update();
        StatusIndicator::update();
    }
}
