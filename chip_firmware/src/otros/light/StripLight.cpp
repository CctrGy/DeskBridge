#include "light/StripLight.h"

#include <Arduino.h>
#include <OneButton.h>

#include "config/DeskConfig.h"
#include "config/pins_config.h"

namespace
{
    constexpr uint16_t pwmMaximum = STRIP_PWM_MAX_DEFAULT;
    constexpr uint32_t pwmFrequencyHz = STRIP_PWM_FREQUENCY_HZ_DEFAULT;
    constexpr uint16_t buttonDebounceMs = STRIP_BUTTON_DEBOUNCE_MS_DEFAULT;
    uint16_t buttonClickMs = STRIP_BUTTON_CLICK_MS_DEFAULT;
    uint16_t buttonLongMs = STRIP_BUTTON_LONG_MS_DEFAULT;
    uint16_t buttonRepeatMs = STRIP_BUTTON_REPEAT_MS_DEFAULT;
    uint16_t brightnessStepValue = STRIP_BRIGHTNESS_STEP_DEFAULT;
    uint16_t kelvinStepValue = STRIP_KELVIN_STEP_DEFAULT;

    OneButton stripButton0(STRIP_BRIGHTNESS_BUTTON_PIN, true, true);
    OneButton stripButton1(STRIP_KELVIN_BUTTON_PIN, true, true);

    bool stripEnabled = STRIP_ENABLED_DEFAULT != 0;
    bool brightnessUp = true;
    bool kelvinUp = true;
    bool coldUp = true;
    bool warmUp = true;
    StripLight::Mode stripMode = StripLight::Mode::Composite;
    uint16_t stripBrightness = STRIP_BRIGHTNESS_DEFAULT;
    uint16_t stripBrightnessMin = STRIP_BRIGHTNESS_MIN_DEFAULT;
    uint16_t stripBrightnessMax = STRIP_BRIGHTNESS_MAX_DEFAULT;

    // 0 = warm white, 1023 = cold white.
    uint16_t stripKelvinMix = STRIP_KELVIN_MIX_DEFAULT;
    uint16_t stripColdMin = STRIP_COLD_MIN_DEFAULT;
    uint16_t stripColdMax = STRIP_COLD_MAX_DEFAULT;
    uint16_t stripHotMin = STRIP_HOT_MIN_DEFAULT;
    uint16_t stripHotMax = STRIP_HOT_MAX_DEFAULT;
    uint16_t appliedColdOutput = 0;
    uint16_t appliedHotOutput = 0;
    uint16_t coldChannelBrightness = STRIP_BRIGHTNESS_DEFAULT;
    uint16_t warmChannelBrightness = STRIP_BRIGHTNESS_DEFAULT;

    StripLight::ButtonAction buttonActions[STRIP_CAPACITIVE_BUTTON_COUNT_DEFAULT] = {
        static_cast<StripLight::ButtonAction>(STRIP_BUTTON_0_ACTION_DEFAULT),
        static_cast<StripLight::ButtonAction>(STRIP_BUTTON_1_ACTION_DEFAULT),
        static_cast<StripLight::ButtonAction>(STRIP_BUTTON_2_ACTION_DEFAULT),
        static_cast<StripLight::ButtonAction>(STRIP_BUTTON_3_ACTION_DEFAULT),
    };

    uint16_t clampPwm(uint16_t value)
    {
        return value > pwmMaximum ? pwmMaximum : value;
    }

    uint16_t scaleChannel(uint16_t value, uint16_t mix)
    {
        return static_cast<uint16_t>((static_cast<uint32_t>(value) * mix) / pwmMaximum);
    }

    uint16_t scaleRange(uint16_t value, uint16_t minimum, uint16_t maximum)
    {
        if (value == 0)
        {
            return 0;
        }

        if (maximum <= minimum)
        {
            return minimum;
        }

        return minimum + static_cast<uint16_t>((static_cast<uint32_t>(value) * (maximum - minimum)) / pwmMaximum);
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

    void setAppliedOutput(uint16_t cold, uint16_t warm)
    {
        appliedColdOutput = cold;
        appliedHotOutput = warm;
        analogWrite(STRIP_COLD_WHITE_PIN, appliedColdOutput);
        analogWrite(STRIP_WARM_WHITE_PIN, appliedHotOutput);
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

    void configureButton(OneButton &button)
    {
        button.setDebounceTicks(buttonDebounceMs);
        button.setClickTicks(buttonClickMs);
        button.setPressMs(buttonLongMs);
        button.setLongPressIntervalMs(buttonRepeatMs);
    }

    void configureButtons()
    {
        configureButton(stripButton0);
        configureButton(stripButton1);
    }

    void applyOutput()
    {
        if (!stripEnabled)
        {
            setAppliedOutput(0, 0);
            return;
        }

        const uint16_t limitedBrightness = constrain(stripBrightness, stripBrightnessMin, stripBrightnessMax);
        const uint16_t activeBrightness = limitedBrightness;
        uint16_t cold = 0;
        uint16_t warm = 0;

        switch (stripMode)
        {
            case StripLight::Mode::OneSimple:
                cold = scaleRange(activeBrightness, stripColdMin, stripColdMax);
                break;
            case StripLight::Mode::TwoSimple:
                cold = scaleRange(constrain(coldChannelBrightness, stripBrightnessMin, stripBrightnessMax), stripColdMin, stripColdMax);
                warm = scaleRange(constrain(warmChannelBrightness, stripBrightnessMin, stripBrightnessMax), stripHotMin, stripHotMax);
                break;
            case StripLight::Mode::Composite:
            default:
                cold = scaleRange(scaleChannel(activeBrightness, stripKelvinMix), stripColdMin, stripColdMax);
                warm = scaleRange(scaleChannel(activeBrightness, pwmMaximum - stripKelvinMix), stripHotMin, stripHotMax);
                break;
        }

        setAppliedOutput(cold, warm);
    }

    void toggleEnabledAction()
    {
        StripLight::toggleEnabled();
    }

    void cycleKelvinPresetAction()
    {
        stripEnabled = true;

        if (stripKelvinMix < 341)
        {
            stripKelvinMix = 512;
        }
        else if (stripKelvinMix < 682)
        {
            stripKelvinMix = pwmMaximum;
        }
        else
        {
            stripKelvinMix = 0;
        }

        applyOutput();
    }

    void moveBrightnessAction()
    {
        stripEnabled = true;

        if (brightnessUp)
        {
            stripBrightness = stripBrightness >= stripBrightnessMax - min(brightnessStepValue, stripBrightnessMax)
                                  ? stripBrightnessMax
                                  : stripBrightness + brightnessStepValue;
        }
        else
        {
            stripBrightness = stripBrightness <= stripBrightnessMin + brightnessStepValue
                                  ? stripBrightnessMin
                                  : stripBrightness - brightnessStepValue;
        }

        applyOutput();
    }

    void moveKelvinAction()
    {
        stripEnabled = true;

        if (kelvinUp)
        {
            stripKelvinMix = stripKelvinMix >= pwmMaximum - kelvinStepValue
                                 ? pwmMaximum
                                 : stripKelvinMix + kelvinStepValue;
        }
        else
        {
            stripKelvinMix = stripKelvinMix <= kelvinStepValue
                                 ? 0
                                 : stripKelvinMix - kelvinStepValue;
        }

        applyOutput();
    }

    void moveColdChannelAction()
    {
        stripEnabled = true;

        if (coldUp)
        {
            coldChannelBrightness = coldChannelBrightness >= stripBrightnessMax - min(brightnessStepValue, stripBrightnessMax)
                                        ? stripBrightnessMax
                                        : coldChannelBrightness + brightnessStepValue;
        }
        else
        {
            coldChannelBrightness = coldChannelBrightness <= stripBrightnessMin + brightnessStepValue
                                        ? stripBrightnessMin
                                        : coldChannelBrightness - brightnessStepValue;
        }

        applyOutput();
    }

    void moveWarmChannelAction()
    {
        stripEnabled = true;

        if (warmUp)
        {
            warmChannelBrightness = warmChannelBrightness >= stripBrightnessMax - min(brightnessStepValue, stripBrightnessMax)
                                        ? stripBrightnessMax
                                        : warmChannelBrightness + brightnessStepValue;
        }
        else
        {
            warmChannelBrightness = warmChannelBrightness <= stripBrightnessMin + brightnessStepValue
                                        ? stripBrightnessMin
                                        : warmChannelBrightness - brightnessStepValue;
        }

        applyOutput();
    }

    void reverseBrightnessDirection()
    {
        brightnessUp = !brightnessUp;
    }

    void reverseKelvinDirection()
    {
        kelvinUp = !kelvinUp;
    }

    void reverseColdDirection()
    {
        coldUp = !coldUp;
    }

    void reverseWarmDirection()
    {
        warmUp = !warmUp;
    }

    StripLight::ButtonAction actionFor(uint8_t index)
    {
        return index < STRIP_CAPACITIVE_BUTTON_COUNT_DEFAULT ? buttonActions[index] : StripLight::ButtonAction::None;
    }

    void buttonClick(uint8_t index)
    {
        switch (actionFor(index))
        {
            case StripLight::ButtonAction::ToggleEnabled:
            case StripLight::ButtonAction::Brightness:
                toggleEnabledAction();
                break;
            case StripLight::ButtonAction::Kelvin:
                cycleKelvinPresetAction();
                break;
            default:
                break;
        }
    }

    void buttonHold(uint8_t index)
    {
        switch (actionFor(index))
        {
            case StripLight::ButtonAction::Brightness:
                moveBrightnessAction();
                break;
            case StripLight::ButtonAction::Kelvin:
                moveKelvinAction();
                break;
            case StripLight::ButtonAction::ColdChannel:
                moveColdChannelAction();
                break;
            case StripLight::ButtonAction::WarmChannel:
                moveWarmChannelAction();
                break;
            default:
                break;
        }
    }

    void buttonStop(uint8_t index)
    {
        switch (actionFor(index))
        {
            case StripLight::ButtonAction::Brightness:
                reverseBrightnessDirection();
                break;
            case StripLight::ButtonAction::Kelvin:
                reverseKelvinDirection();
                break;
            case StripLight::ButtonAction::ColdChannel:
                reverseColdDirection();
                break;
            case StripLight::ButtonAction::WarmChannel:
                reverseWarmDirection();
                break;
            default:
                break;
        }
    }

    void button0Click()
    {
        buttonClick(0);
    }

    void button0Hold()
    {
        buttonHold(0);
    }

    void button0Stop()
    {
        buttonStop(0);
    }

    void button1Click()
    {
        buttonClick(1);
    }

    void button1Hold()
    {
        buttonHold(1);
    }

    void button1Stop()
    {
        buttonStop(1);
    }
}

namespace StripLight
{
    void begin()
    {
        analogWriteResolution(10);
        analogWriteFrequency(pwmFrequencyHz);
        pinMode(STRIP_COLD_WHITE_PIN, OUTPUT);
        pinMode(STRIP_WARM_WHITE_PIN, OUTPUT);

        configureButtons();
        stripButton0.attachClick(button0Click);
        stripButton0.attachDuringLongPress(button0Hold);
        stripButton0.attachLongPressStop(button0Stop);

        stripButton1.attachClick(button1Click);
        stripButton1.attachDuringLongPress(button1Hold);
        stripButton1.attachLongPressStop(button1Stop);

        applyOutput();
    }

    void update()
    {
        stripButton0.tick();
        stripButton1.tick();
    }

    bool enabled()
    {
        return stripEnabled;
    }

    void setEnabled(bool value)
    {
        stripEnabled = value;
        applyOutput();
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
        applyOutput();
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

    ButtonAction buttonAction(uint8_t index)
    {
        return actionFor(index);
    }

    void setButtonAction(uint8_t index, ButtonAction action)
    {
        if (index >= STRIP_CAPACITIVE_BUTTON_COUNT_DEFAULT)
        {
            return;
        }

        buttonActions[index] = action;
    }

    const char *buttonActionName(ButtonAction action)
    {
        switch (action)
        {
            case ButtonAction::ToggleEnabled:
                return "Toggle";
            case ButtonAction::Brightness:
                return "Brightness";
            case ButtonAction::Kelvin:
                return "Kelvin";
            case ButtonAction::ColdChannel:
                return "Cold";
            case ButtonAction::WarmChannel:
                return "Warm";
            case ButtonAction::None:
            default:
                return "None";
        }
    }

    uint8_t buttonCount()
    {
        return STRIP_CAPACITIVE_BUTTON_COUNT_DEFAULT;
    }

    uint16_t brightness()
    {
        return stripBrightness;
    }

    void setBrightness(uint16_t value)
    {
        stripBrightness = constrain(clampPwm(value), stripBrightnessMin, stripBrightnessMax);
        applyOutput();
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
        applyOutput();
    }

    uint8_t kelvinColdPercent()
    {
        return static_cast<uint8_t>((static_cast<uint32_t>(stripKelvinMix) * 100) / pwmMaximum);
    }

    void setKelvinColdPercent(uint8_t percent)
    {
        stripKelvinMix = static_cast<uint16_t>((static_cast<uint32_t>(constrain(percent, 0, 100)) * pwmMaximum) / 100);
        applyOutput();
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
        applyOutput();
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
        applyOutput();
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
        applyOutput();
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
        applyOutput();
    }

    uint16_t buttonClickTime()
    {
        return buttonClickMs;
    }

    void setButtonClickTime(uint16_t value)
    {
        buttonClickMs = constrain(value, STRIP_BUTTON_CLICK_MS_MIN_DEFAULT, STRIP_BUTTON_CLICK_MS_MAX_DEFAULT);
        configureButtons();
    }

    uint16_t buttonLongTime()
    {
        return buttonLongMs;
    }

    void setButtonLongTime(uint16_t value)
    {
        buttonLongMs = constrain(value, STRIP_BUTTON_LONG_MS_MIN_DEFAULT, STRIP_BUTTON_LONG_MS_MAX_DEFAULT);
        configureButtons();
    }

    uint16_t buttonDuringTime()
    {
        return buttonRepeatMs;
    }

    void setButtonDuringTime(uint16_t value)
    {
        buttonRepeatMs = constrain(value, STRIP_BUTTON_REPEAT_MS_MIN_DEFAULT, STRIP_BUTTON_REPEAT_MS_MAX_DEFAULT);
        configureButtons();
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

    uint16_t coldOutput()
    {
        return appliedColdOutput;
    }

    uint16_t hotOutput()
    {
        return appliedHotOutput;
    }

    uint16_t pwmMax()
    {
        return pwmMaximum;
    }

    void resetDefaults()
    {
        stripEnabled = STRIP_ENABLED_DEFAULT != 0;
        brightnessUp = true;
        kelvinUp = true;
        coldUp = true;
        warmUp = true;
        stripMode = defaultMode();
        stripBrightness = STRIP_BRIGHTNESS_DEFAULT;
        stripBrightnessMin = STRIP_BRIGHTNESS_MIN_DEFAULT;
        stripBrightnessMax = STRIP_BRIGHTNESS_MAX_DEFAULT;
        stripKelvinMix = STRIP_KELVIN_MIX_DEFAULT;
        stripColdMin = STRIP_COLD_MIN_DEFAULT;
        stripColdMax = STRIP_COLD_MAX_DEFAULT;
        stripHotMin = STRIP_HOT_MIN_DEFAULT;
        stripHotMax = STRIP_HOT_MAX_DEFAULT;
        coldChannelBrightness = STRIP_BRIGHTNESS_DEFAULT;
        warmChannelBrightness = STRIP_BRIGHTNESS_DEFAULT;
        buttonClickMs = STRIP_BUTTON_CLICK_MS_DEFAULT;
        buttonLongMs = STRIP_BUTTON_LONG_MS_DEFAULT;
        buttonRepeatMs = STRIP_BUTTON_REPEAT_MS_DEFAULT;
        brightnessStepValue = STRIP_BRIGHTNESS_STEP_DEFAULT;
        kelvinStepValue = STRIP_KELVIN_STEP_DEFAULT;
        buttonActions[0] = static_cast<ButtonAction>(STRIP_BUTTON_0_ACTION_DEFAULT);
        buttonActions[1] = static_cast<ButtonAction>(STRIP_BUTTON_1_ACTION_DEFAULT);
        buttonActions[2] = static_cast<ButtonAction>(STRIP_BUTTON_2_ACTION_DEFAULT);
        buttonActions[3] = static_cast<ButtonAction>(STRIP_BUTTON_3_ACTION_DEFAULT);
        configureButtons();
        applyOutput();
    }
}
