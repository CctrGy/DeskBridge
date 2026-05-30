#include "OledDisplay.hpp"

#include "config/pins_config.h"

OledDisplay Oled;

OledDisplay::OledDisplay()
    : display_(U8G2_R0, OLED_CS_PIN, OLED_DC_PIN, OLED_RST_PIN)
{
}

void OledDisplay::begin()
{
    display_.begin();
}

void OledDisplay::clearBuffer()
{
    display_.clearBuffer();
}

void OledDisplay::sendBuffer()
{
    display_.sendBuffer();
}

void OledDisplay::setFont(Font font)
{
    display_.setFont(font);
}

void OledDisplay::setFont(OledFont font)
{
    switch (font)
    {
        case OledFont::Tiny:
            display_.setFont(u8g2_font_5x7_tf);
            break;
        case OledFont::Small:
            display_.setFont(u8g2_font_5x8_tf);
            break;
        case OledFont::Text:
            display_.setFont(u8g2_font_6x10_tf);
            break;
        case OledFont::Date:
            display_.setFont(u8g2_font_6x12_tf);
            break;
        case OledFont::Value:
            display_.setFont(u8g2_font_logisoso18_tf);
            break;
        case OledFont::HomeTime:
            display_.setFont(u8g2_font_logisoso20_tf);
            break;
        case OledFont::BootTitle:
            display_.setFont(u8g2_font_10x20_tf);
            break;
    }
}

void OledDisplay::setDrawColor(uint8_t color)
{
    display_.setDrawColor(color);
}

void OledDisplay::setCursor(int16_t x, int16_t y)
{
    display_.setCursor(x, y);
}

int OledDisplay::getStrWidth(const char *text)
{
    return display_.getStrWidth(text);
}

void OledDisplay::drawStr(int16_t x, int16_t y, const char *text)
{
    display_.drawStr(x, y, text);
}

void OledDisplay::drawHLine(int16_t x, int16_t y, uint16_t width)
{
    display_.drawHLine(x, y, width);
}

void OledDisplay::drawFrame(int16_t x, int16_t y, uint16_t width, uint16_t height)
{
    display_.drawFrame(x, y, width, height);
}

void OledDisplay::drawBox(int16_t x, int16_t y, uint16_t width, uint16_t height)
{
    display_.drawBox(x, y, width, height);
}

size_t OledDisplay::print(const char *text)
{
    return display_.print(text);
}

size_t OledDisplay::print(char value)
{
    return display_.print(value);
}

size_t OledDisplay::print(int value)
{
    return display_.print(value);
}

size_t OledDisplay::print(unsigned int value)
{
    return display_.print(value);
}

size_t OledDisplay::print(long value)
{
    return display_.print(value);
}

size_t OledDisplay::print(unsigned long value)
{
    return display_.print(value);
}

size_t OledDisplay::print(uint8_t value)
{
    return display_.print(static_cast<unsigned int>(value));
}

size_t OledDisplay::print(uint16_t value)
{
    return display_.print(static_cast<unsigned int>(value));
}

size_t OledDisplay::print(int16_t value)
{
    return display_.print(static_cast<int>(value));
}

size_t OledDisplay::print(float value)
{
    return display_.print(value);
}

size_t OledDisplay::print(uint8_t value, int base)
{
    return display_.print(static_cast<unsigned int>(value), base);
}

size_t OledDisplay::print(uint16_t value, int base)
{
    return display_.print(static_cast<unsigned int>(value), base);
}

size_t OledDisplay::print(uint32_t value, int base)
{
    return display_.print(static_cast<unsigned long>(value), base);
}
