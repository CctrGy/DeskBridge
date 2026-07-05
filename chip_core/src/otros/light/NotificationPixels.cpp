#include "light/NotificationPixels.h"

#include <Adafruit_NeoPixel.h>
#include <math.h>

#include "bus/DeskBus.h"
#include "bus/KeypadPeripheral.h"
#include "config/DeskConfig.h"
#include "config/pins_config.h"
#include "usb/DeskUSB.h"
#include <Antenna.hpp>

namespace
{
    Adafruit_NeoPixel pixels(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
    uint8_t pixelBrightness = NOTIFICATION_PIXEL_BRIGHTNESS_DEFAULT;
    uint8_t notificationMode = NOTIFICATION_MODE_DEFAULT;
    uint32_t lastPixelColor[NEOPIXEL_COUNT] = {};
    bool pixelStateKnown = false;
    uint32_t keypadEventPulseUntilMs = 0;
    uint8_t keypadEventButton = 0;
    uint8_t keypadEventCode = 0;

    constexpr uint32_t bootColors[] = {
        0xFF0000,
        0x00FF00,
        0x0000FF,
    };

    uint32_t makeColor(uint8_t red, uint8_t green, uint8_t blue)
    {
        return pixels.Color(red, green, blue);
    }

    bool blinkOn()
    {
        return ((millis() / NOTIFICATION_BLINK_MS_DEFAULT) % 2U) == 0U;
    }

    bool keypadConfigured()
    {
        return KEYPAD_UART_TX_PIN != NC || KEYPAD_UART_RX_PIN != NC || KEYPAD_STATE_PIN != NC || KEYPAD_EVENT_PIN != NC;
    }

    bool anySensorMissing()
    {
        return !DeskBus::ready(DeskBus::Device::ENS160) ||
               !DeskBus::ready(DeskBus::Device::AHT21) ||
               !DeskBus::ready(DeskBus::Device::VEML7700) ||
               !DeskBus::ready(DeskBus::Device::SCD41) ||
               !DeskBus::ready(DeskBus::Device::SHT21);
    }

    bool anySensorFault()
    {
        if (millis() < 7000)
        {
            return false;
        }

        const DeskBus::Measurements &data = DeskBus::measurements();
        return (DeskBus::ready(DeskBus::Device::AHT21) && (isnan(data.temperature) || isnan(data.humidity))) ||
               (DeskBus::ready(DeskBus::Device::VEML7700) && isnan(data.lux)) ||
               (DeskBus::ready(DeskBus::Device::SCD41) && data.co2Ppm < 0) ||
               (DeskBus::ready(DeskBus::Device::SHT21) && (isnan(data.shtTemperature) || isnan(data.shtHumidity)));
    }

    bool systemBasicsFault()
    {
        return !DeskBus::ready(DeskBus::Device::DS3231) || !DeskBus::ready(DeskBus::Device::AT24C32);
    }

    bool keypadFault()
    {
        return keypadConfigured() && !KeypadPeripheral::present();
    }

    bool wirelessFault()
    {
        return WirelessAntenna.state() == Antenna::State::Error || WirelessAntenna.state() == Antenna::State::Unsupported;
    }

    void setPixelCached(uint8_t index, uint32_t color, bool &changed)
    {
        if (index >= NEOPIXEL_COUNT)
        {
            return;
        }

        if (!pixelStateKnown || lastPixelColor[index] != color)
        {
            pixels.setPixelColor(index, color);
            lastPixelColor[index] = color;
            changed = true;
        }
    }
}

namespace NotificationPixels
{
    void begin()
    {
        if (NEOPIXEL_PIN == NC)
        {
            return;
        }

        pixels.begin();
        pixels.setBrightness(pixelBrightness);
        clear();
    }

    void update()
    {
        if (NEOPIXEL_PIN == NC)
        {
            return;
        }

        bool changed = false;
        const bool blink = blinkOn();
        const bool systemFault = systemBasicsFault();
        const bool sensorFault = anySensorFault();
        const bool sensorsMissing = anySensorMissing();
        const bool missingKeypad = keypadFault();
        const bool badWireless = wirelessFault();
        const bool usbMounted = DeskUSB::mounted();
        const bool usbSerialOpen = DeskUSB::cdcConnected(DeskUSB::CONTROL);

        uint32_t systemColor = 0;
        if (systemFault || sensorFault)
        {
            systemColor = blink ? makeColor(NOTIFICATION_ERROR_RED_DEFAULT,
                                            NOTIFICATION_ERROR_GREEN_DEFAULT,
                                            NOTIFICATION_ERROR_BLUE_DEFAULT) : 0;
        }
        else if (sensorsMissing)
        {
            systemColor = makeColor(NOTIFICATION_WARNING_RED_DEFAULT,
                                    NOTIFICATION_WARNING_GREEN_DEFAULT,
                                    NOTIFICATION_WARNING_BLUE_DEFAULT);
        }

        uint32_t keypadColor = 0;
        if (keypadEventPulseUntilMs > millis())
        {
            const uint8_t red = keypadEventCode == 3 ? 255 : static_cast<uint8_t>(40 + (keypadEventButton * 35));
            const uint8_t green = keypadEventCode == 1 ? 180 : 80;
            const uint8_t blue = keypadEventCode == 2 ? 255 : 120;
            keypadColor = makeColor(red, green, blue);
        }
        else if (KeypadPeripheral::stateActive())
        {
            keypadColor = makeColor(NOTIFICATION_KEYPAD_BUTTON_RED_DEFAULT,
                                    NOTIFICATION_KEYPAD_BUTTON_GREEN_DEFAULT,
                                    NOTIFICATION_KEYPAD_BUTTON_BLUE_DEFAULT);
        }
        else if (missingKeypad)
        {
            keypadColor = blink ? makeColor(NOTIFICATION_KEYPAD_MISSING_RED_DEFAULT,
                                            NOTIFICATION_KEYPAD_MISSING_GREEN_DEFAULT,
                                            NOTIFICATION_KEYPAD_MISSING_BLUE_DEFAULT) : 0;
        }

        uint32_t communicationColor = 0;
        if (badWireless)
        {
            communicationColor = blink ? makeColor(NOTIFICATION_WIRELESS_ERROR_RED_DEFAULT,
                                                   NOTIFICATION_WIRELESS_ERROR_GREEN_DEFAULT,
                                                   NOTIFICATION_WIRELESS_ERROR_BLUE_DEFAULT) : 0;
        }
        else if (usbSerialOpen)
        {
            communicationColor = makeColor(NOTIFICATION_USB_OPEN_RED_DEFAULT,
                                           NOTIFICATION_USB_OPEN_GREEN_DEFAULT,
                                           NOTIFICATION_USB_OPEN_BLUE_DEFAULT);
        }
        else if (WirelessAntenna.bleConnected())
        {
            communicationColor = makeColor(NOTIFICATION_BLE_CONNECTED_RED_DEFAULT,
                                           NOTIFICATION_BLE_CONNECTED_GREEN_DEFAULT,
                                           NOTIFICATION_BLE_CONNECTED_BLUE_DEFAULT);
        }
        else if (WirelessAntenna.enabled() && WirelessAntenna.mode() == Antenna::Mode::BleDevice)
        {
            communicationColor = makeColor(NOTIFICATION_BLE_ADVERTISING_RED_DEFAULT,
                                           NOTIFICATION_BLE_ADVERTISING_GREEN_DEFAULT,
                                           NOTIFICATION_BLE_ADVERTISING_BLUE_DEFAULT);
        }
        else if (usbMounted)
        {
            communicationColor = makeColor(NOTIFICATION_USB_MOUNTED_RED_DEFAULT,
                                           NOTIFICATION_USB_MOUNTED_GREEN_DEFAULT,
                                           NOTIFICATION_USB_MOUNTED_BLUE_DEFAULT);
        }

        setPixelCached(NOTIFICATION_SYSTEM_BUS_PIXEL_DEFAULT, systemColor, changed);
        setPixelCached(NOTIFICATION_KEYPAD_BUTTON_PIXEL_DEFAULT, keypadColor, changed);
        setPixelCached(NOTIFICATION_USB_PIXEL_DEFAULT, communicationColor, changed);

        if (changed)
        {
            pixelStateKnown = true;
            pixels.show();
        }
    }

    void clear()
    {
        if (NEOPIXEL_PIN == NC)
        {
            return;
        }

        pixelStateKnown = false;
        for (uint8_t index = 0; index < NEOPIXEL_COUNT; ++index)
        {
            lastPixelColor[index] = 0;
        }
        pixels.clear();
        pixels.show();
    }

    void showBootStep(uint8_t step)
    {
        if (NEOPIXEL_PIN == NC)
        {
            return;
        }

        constexpr uint8_t colorCount = sizeof(bootColors) / sizeof(bootColors[0]);
        const uint8_t colorIndex = step / NEOPIXEL_COUNT;
        const uint8_t pixelIndex = step % NEOPIXEL_COUNT;

        if (colorIndex >= colorCount)
        {
            return;
        }

        pixels.clear();
        pixels.setPixelColor(pixelIndex, bootColors[colorIndex]);
        pixels.show();
    }

    void showKeypadEvent(uint8_t button, uint8_t eventCode)
    {
        keypadEventButton = button;
        keypadEventCode = eventCode;
        keypadEventPulseUntilMs = millis() + 180U;
    }

    void set(uint8_t index, uint8_t red, uint8_t green, uint8_t blue)
    {
        if (NEOPIXEL_PIN == NC)
        {
            return;
        }

        if (index >= NEOPIXEL_COUNT)
        {
            return;
        }

        pixels.setPixelColor(index, pixels.Color(red, green, blue));
        lastPixelColor[index] = pixels.Color(red, green, blue);
        pixelStateKnown = true;
        pixels.show();
    }

    uint8_t brightness()
    {
        return pixelBrightness;
    }

    void setBrightness(uint8_t value)
    {
        pixelBrightness = value;
        if (NEOPIXEL_PIN == NC)
        {
            return;
        }

        pixels.setBrightness(pixelBrightness);
        pixels.show();
    }

    uint8_t mode()
    {
        return notificationMode;
    }

    void setMode(uint8_t value)
    {
        notificationMode = value % NOTIFICATION_MODE_COUNT_DEFAULT;
    }
}
