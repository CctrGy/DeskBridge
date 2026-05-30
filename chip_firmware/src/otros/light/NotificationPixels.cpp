#include "light/NotificationPixels.h"

#include <Adafruit_NeoPixel.h>

#include "config/DeskConfig.h"
#include "config/pins_config.h"

namespace
{
    Adafruit_NeoPixel pixels(NEOPIXEL_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);
    uint8_t pixelBrightness = NOTIFICATION_PIXEL_BRIGHTNESS_DEFAULT;
    uint8_t notificationMode = NOTIFICATION_MODE_DEFAULT;

    constexpr uint32_t bootColors[] = {
        0xFF0000,
        0x00FF00,
        0x0000FF,
    };
}

namespace NotificationPixels
{
    void begin()
    {
        pixels.begin();
        pixels.setBrightness(pixelBrightness);
        clear();
    }

    void clear()
    {
        pixels.clear();
        pixels.show();
    }

    void showBootStep(uint8_t step)
    {
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

    void set(uint8_t index, uint8_t red, uint8_t green, uint8_t blue)
    {
        if (index >= NEOPIXEL_COUNT)
        {
            return;
        }

        pixels.setPixelColor(index, pixels.Color(red, green, blue));
        pixels.show();
    }

    uint8_t brightness()
    {
        return pixelBrightness;
    }

    void setBrightness(uint8_t value)
    {
        pixelBrightness = value;
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
