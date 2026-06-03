#pragma once

#include "OledDisplay.hpp"

class Window
{
public:
    explicit Window(OledDisplay &oled = Oled)
        : oled_(oled)
    {
    }

    void beginFrame()
    {
        oled_.clearBuffer();
    }

    void endFrame()
    {
        oled_.sendBuffer();
    }

    void title(const char *text)
    {
        oled_.setFont(OledFont::Small);
        oled_.drawStr(0, 7, text);
        oled_.drawHLine(0, 9, 128);
    }

    void text(int16_t x, int16_t y, const char *value, OledFont font = OledFont::Small)
    {
        oled_.setFont(font);
        oled_.drawStr(x, y, value);
    }

    void value(int16_t x, int16_t y, const char *valueText)
    {
        text(x, y, valueText, OledFont::Value);
    }

    void selectedRow(int16_t y)
    {
        oled_.drawBox(0, y - 7, 128, 9);
        oled_.setDrawColor(0);
    }

    void normalColor()
    {
        oled_.setDrawColor(1);
    }

private:
    OledDisplay &oled_;
};
