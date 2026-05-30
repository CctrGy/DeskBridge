#include "light/StripLight.h"

#include <Arduino.h>

#include "config/DeskConfig.h"

namespace
{
    constexpr uint16_t pwmMaximum = STRIP_PWM_MAX_DEFAULT;
    uint16_t buttonClickMs = STRIP_BUTTON_CLICK_MS_DEFAULT;
    uint16_t buttonLongMs = STRIP_BUTTON_LONG_MS_DEFAULT;
    uint16_t buttonRepeatMs = STRIP_BUTTON_REPEAT_MS_DEFAULT;
    uint16_t brightnessStepValue = STRIP_BRIGHTNESS_STEP_DEFAULT;
    uint16_t kelvinStepValue = STRIP_KELVIN_STEP_DEFAULT;

    bool stripEnabled = STRIP_ENABLED_DEFAULT != 0;
    StripLight::Mode stripMode = StripLight::Mode::Composite;
    uint16_t stripBrightness = STRIP_BRIGHTNESS_DEFAULT;
    uint16_t stripBrightnessMin = STRIP_BRIGHTNESS_MIN_DEFAULT;
    uint16_t stripBrightnessMax = STRIP_BRIGHTNESS_MAX_DEFAULT;

    // 0 = warm white, 1023 = cold white. This is configuration only now.
    uint16_t stripKelvinMix = STRIP_KELVIN_MIX_DEFAULT;
    uint16_t stripColdMin = STRIP_COLD_MIN_DEFAULT;
    uint16_t stripColdMax = STRIP_COLD_MAX_DEFAULT;
    uint16_t stripHotMin = STRIP_HOT_MIN_DEFAULT;
    uint16_t stripHotMax = STRIP_HOT_MAX_DEFAULT;

    uint16_t clampPwm(uint16_t value)
    {
        return value > pwmMaximum ? pwmMaximum : value;
    }

    uint16_t scalePercentToPwm(uint8_t percent, uint16_t minimum, uint16_t maximum)
    {
        const uint8_t limitedPercent = constrain(percent, 0, 100);
        if (maximum <= minimum)
        {
            return minimum;
        }

        return minimum + static_cast<uint16_t>((static_cast<uint32_t>(limitedPercent) * (maximum - minimum)) / 100);
    }

    uint8_t scalePwmToPercent(uint16_t value, uint16_t minimum, uint16_t maximum)
    {
        if (maximum <= minimum)
        {
            return 0;
        }

        const uint16_t limitedValue = constrain(value, minimum, maximum);
        return static_cast<uint8_t>((static_cast<uint32_t>(limitedValue - minimum) * 100) / (maximum - minimum));
    }

    StripLight::Mode defaultMode()
    {
        switch (STRIP_MODE_DEFAULT)
        {
            case 0:
                return StripLight::Mode::OneSimple;
            case 1:
                return StripLight::Mode::TwoSimple;
            default:
                return StripLight::Mode::Composite;
        }
    }
}

namespace StripLight
{
    void begin()
    {
    }

    void update()
    {
    }

    bool enabled()
    {
        return stripEnabled;
    }

    void setEnabled(bool value)
    {
        stripEnabled = value;
    }

    void toggleEnabled()
    {
        setEnabled(!stripEnabled);
    }

    Mode mode()
    {
        return stripMode;
    }

    void setMode(Mode value)
    {
        stripMode = value;
    }

    void toggleMode()
    {
        switch (stripMode)
        {
            case Mode::OneSimple:
                setMode(Mode::TwoSimple);
                break;
            case Mode::TwoSimple:
                setMode(Mode::Composite);
                break;
            case Mode::Composite:
            default:
                setMode(Mode::OneSimple);
                break;
        }
    }

    const char *modeName()
    {
        switch (stripMode)
        {
            case Mode::OneSimple:
                return "OneSimple";
            case Mode::TwoSimple:
                return "TwoSimple";
            case Mode::Composite:
            default:
                return "Composite";
        }
    }

    uint16_t brightness()
    {
        return stripBrightness;
    }

    void setBrightness(uint16_t value)
    {
        stripBrightness = constrain(clampPwm(value), stripBrightnessMin, stripBrightnessMax);
    }

    uint8_t brightnessPercent()
    {
        return scalePwmToPercent(stripBrightness, stripBrightnessMin, stripBrightnessMax);
    }

    void setBrightnessPercent(uint8_t percent)
    {
        setBrightness(scalePercentToPwm(percent, stripBrightnessMin, stripBrightnessMax));
    }

    uint16_t brightnessMin()
    {
        return stripBrightnessMin;
    }

    void setBrightnessMin(uint16_t value)
    {
        stripBrightnessMin = clampPwm(value);
        if (stripBrightnessMin > stripBrightnessMax)
        {
            stripBrightnessMax = stripBrightnessMin;
        }
        setBrightness(stripBrightness);
    }

    uint16_t brightnessMax()
    {
        return stripBrightnessMax;
    }

    void setBrightnessMax(uint16_t value)
    {
        stripBrightnessMax = clampPwm(value);
        if (stripBrightnessMax < stripBrightnessMin)
        {
            stripBrightnessMin = stripBrightnessMax;
        }
        setBrightness(stripBrightness);
    }

    uint16_t kelvinMix()
    {
        return stripKelvinMix;
    }

    void setKelvinMix(uint16_t value)
    {
        stripKelvinMix = clampPwm(value);
    }

    uint8_t kelvinColdPercent()
    {
        return static_cast<uint8_t>((static_cast<uint32_t>(stripKelvinMix) * 100) / pwmMaximum);
    }

    void setKelvinColdPercent(uint8_t percent)
    {
        stripKelvinMix = static_cast<uint16_t>((static_cast<uint32_t>(constrain(percent, 0, 100)) * pwmMaximum) / 100);
    }

    void setKelvinWarm()
    {
        setKelvinMix(0);
    }

    void setKelvinNeutral()
    {
        setKelvinMix(pwmMaximum / 2);
    }

    void setKelvinCold()
    {
        setKelvinMix(pwmMaximum);
    }

    uint16_t coldMin()
    {
        return stripColdMin;
    }

    void setColdMin(uint16_t value)
    {
        stripColdMin = clampPwm(value);
        if (stripColdMin > stripColdMax)
        {
            stripColdMax = stripColdMin;
        }
    }

    uint16_t coldMax()
    {
        return stripColdMax;
    }

    void setColdMax(uint16_t value)
    {
        stripColdMax = clampPwm(value);
        if (stripColdMax < stripColdMin)
        {
            stripColdMin = stripColdMax;
        }
    }

    uint16_t hotMin()
    {
        return stripHotMin;
    }

    void setHotMin(uint16_t value)
    {
        stripHotMin = clampPwm(value);
        if (stripHotMin > stripHotMax)
        {
            stripHotMax = stripHotMin;
        }
    }

    uint16_t hotMax()
    {
        return stripHotMax;
    }

    void setHotMax(uint16_t value)
    {
        stripHotMax = clampPwm(value);
        if (stripHotMax < stripHotMin)
        {
            stripHotMin = stripHotMax;
        }
    }

    uint16_t buttonClickTime()
    {
        return buttonClickMs;
    }

    void setButtonClickTime(uint16_t value)
    {
        buttonClickMs = constrain(value, STRIP_BUTTON_CLICK_MS_MIN_DEFAULT, STRIP_BUTTON_CLICK_MS_MAX_DEFAULT);
    }

    uint16_t buttonLongTime()
    {
        return buttonLongMs;
    }

    void setButtonLongTime(uint16_t value)
    {
        buttonLongMs = constrain(value, STRIP_BUTTON_LONG_MS_MIN_DEFAULT, STRIP_BUTTON_LONG_MS_MAX_DEFAULT);
    }

    uint16_t buttonDuringTime()
    {
        return buttonRepeatMs;
    }

    void setButtonDuringTime(uint16_t value)
    {
        buttonRepeatMs = constrain(value, STRIP_BUTTON_REPEAT_MS_MIN_DEFAULT, STRIP_BUTTON_REPEAT_MS_MAX_DEFAULT);
    }

    uint16_t brightnessStep()
    {
        return brightnessStepValue;
    }

    void setBrightnessStep(uint16_t value)
    {
        brightnessStepValue = constrain(value, STRIP_STEP_MIN_DEFAULT, STRIP_STEP_MAX_DEFAULT);
    }

    uint16_t kelvinStep()
    {
        return kelvinStepValue;
    }

    void setKelvinStep(uint16_t value)
    {
        kelvinStepValue = constrain(value, STRIP_STEP_MIN_DEFAULT, STRIP_STEP_MAX_DEFAULT);
    }

    uint16_t pwmMax()
    {
        return pwmMaximum;
    }

    void resetDefaults()
    {
        stripEnabled = STRIP_ENABLED_DEFAULT != 0;
        stripMode = defaultMode();
        stripBrightness = STRIP_BRIGHTNESS_DEFAULT;
        stripBrightnessMin = STRIP_BRIGHTNESS_MIN_DEFAULT;
        stripBrightnessMax = STRIP_BRIGHTNESS_MAX_DEFAULT;
        stripKelvinMix = STRIP_KELVIN_MIX_DEFAULT;
        stripColdMin = STRIP_COLD_MIN_DEFAULT;
        stripColdMax = STRIP_COLD_MAX_DEFAULT;
        stripHotMin = STRIP_HOT_MIN_DEFAULT;
        stripHotMax = STRIP_HOT_MAX_DEFAULT;
        buttonClickMs = STRIP_BUTTON_CLICK_MS_DEFAULT;
        buttonLongMs = STRIP_BUTTON_LONG_MS_DEFAULT;
        buttonRepeatMs = STRIP_BUTTON_REPEAT_MS_DEFAULT;
        brightnessStepValue = STRIP_BRIGHTNESS_STEP_DEFAULT;
        kelvinStepValue = STRIP_KELVIN_STEP_DEFAULT;
    }
}
