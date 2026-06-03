#pragma once

#include <stdint.h>

namespace StripLight
{
    enum class Mode : uint8_t
    {
        OneSimple,
        TwoSimple,
        Composite,

        // Compatibility aliases for the current menu/settings code.
        Simple = OneSimple,
        Dual = Composite,
    };

    void begin();
    void update();

    bool enabled();
    void setEnabled(bool value);
    void toggleEnabled();

    Mode mode();
    void setMode(Mode value);
    void toggleMode();
    const char *modeName();

    uint16_t brightness();
    void setBrightness(uint16_t value);
    uint8_t brightnessPercent();
    void setBrightnessPercent(uint8_t percent);
    uint16_t brightnessMin();
    void setBrightnessMin(uint16_t value);
    uint16_t brightnessMax();
    void setBrightnessMax(uint16_t value);

    uint16_t kelvinMix();
    void setKelvinMix(uint16_t value);
    uint8_t kelvinColdPercent();
    void setKelvinColdPercent(uint8_t percent);
    void setKelvinWarm();
    void setKelvinNeutral();
    void setKelvinCold();
    uint16_t coldMin();
    void setColdMin(uint16_t value);
    uint16_t coldMax();
    void setColdMax(uint16_t value);
    uint16_t hotMin();
    void setHotMin(uint16_t value);
    uint16_t hotMax();
    void setHotMax(uint16_t value);

    uint16_t buttonClickTime();
    void setButtonClickTime(uint16_t value);
    uint16_t buttonLongTime();
    void setButtonLongTime(uint16_t value);
    uint16_t buttonDuringTime();
    void setButtonDuringTime(uint16_t value);

    uint16_t brightnessStep();
    void setBrightnessStep(uint16_t value);

    uint16_t kelvinStep();
    void setKelvinStep(uint16_t value);

    uint16_t pwmMax();
    void resetDefaults();
}
