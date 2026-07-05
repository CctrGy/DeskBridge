#pragma once

#include <Arduino.h>
#include <U8g2lib.h>

enum class OledFont : uint8_t
{
    Tiny,
    Small,
    Text,
    Date,
    Value,
    HomeTime,
    BootTitle,
};

class OledDisplay
{
public:
    using Font = const uint8_t *;

    OledDisplay();

    void begin();
    void setPowerSave(bool enabled);
    void clearBuffer();
    void sendBuffer();
    void setFont(Font font);
    void setFont(OledFont font);
    void setDrawColor(uint8_t color);
    void setCursor(int16_t x, int16_t y);

    int getStrWidth(const char *text);
    void drawStr(int16_t x, int16_t y, const char *text);
    void drawHLine(int16_t x, int16_t y, uint16_t width);
    void drawFrame(int16_t x, int16_t y, uint16_t width, uint16_t height);
    void drawBox(int16_t x, int16_t y, uint16_t width, uint16_t height);

    size_t print(const char *text);
    size_t print(char value);
    size_t print(int value);
    size_t print(unsigned int value);
    size_t print(long value);
    size_t print(unsigned long value);
    size_t print(uint8_t value);
    size_t print(uint16_t value);
    size_t print(int16_t value);
    size_t print(float value);
    size_t print(uint8_t value, int base);
    size_t print(uint16_t value, int base);
    size_t print(uint32_t value, int base);

private:
    U8G2_SSD1309_128X64_NONAME2_F_4W_HW_SPI display_;
};

extern OledDisplay Oled;
