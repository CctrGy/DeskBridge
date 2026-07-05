#include "input/ButtonControls.h"

#include "core/PeripheralState.h"

namespace
{
    constexpr uint8_t STRIP_MODE_SINGLE_MASK = (1 << static_cast<uint8_t>(StripMode::ColdOnly)) | (1 << static_cast<uint8_t>(StripMode::WarmOnly));
    constexpr uint8_t STRIP_MODE_DUAL_MASK = (1 << static_cast<uint8_t>(StripMode::DualWhite));
    constexpr uint8_t STRIP_MODE_ANY_MASK = STRIP_MODE_SINGLE_MASK | STRIP_MODE_DUAL_MASK;

    ButtonTimingConfig sharedTiming;

    bool powerLongActive = false;
    bool adjustLongActive = false;

    uint16_t stepValue(uint16_t value, uint16_t step, bool up, uint16_t minimum, uint16_t maximum)
    {
        if (up)
        {
            if (value >= maximum || maximum - value <= step)
            {
                return maximum;
            }
            return value + step;
        }

        if (value <= minimum || value - minimum <= step)
        {
            return minimum;
        }
        return value - step;
    }

    void handlePowerClick()
    {
        State.stripEnabled = !State.stripEnabled;
    }

    void handleAdjustClick()
    {
    }

    void handleVolumeMutedClick()
    {
        setCoreAction("A001");
        markEvent(EVENT_CORE_COMMAND);
    }

    void handleVolumeUpClick()
    {
        setCoreAction("A002");
        markEvent(EVENT_CORE_COMMAND);
    }

    void handleVolumeDownClick()
    {
        setCoreAction("A003");
        markEvent(EVENT_CORE_COMMAND);
    }

    void handlePowerLongStart()
    {
        powerLongActive = true;
        markEvent(EVENT_POWER_LONG);
    }

    void handleAdjustLongStart()
    {
        adjustLongActive = true;
        markEvent(EVENT_ADJUST_LONG);
    }

    void handlePowerLongStop()
    {
        if (!powerLongActive)
        {
            return;
        }

        State.brightnessUp = !State.brightnessUp;
        powerLongActive = false;
    }

    void handleAdjustLongStop()
    {
        if (!adjustLongActive)
        {
            return;
        }

        State.kelvinUp = !State.kelvinUp;
        adjustLongActive = false;
    }

    void handlePowerDuringLong()
    {
        State.stripEnabled = true;
        State.brightness = stepValue(State.brightness, State.brightnessStep, State.brightnessUp, 0, PeripheralConfig::PWM_MAX);
    }

    void handleAdjustDuringLong()
    {
        State.stripEnabled = true;
        State.kelvin = stepValue(State.kelvin, State.kelvinStep, State.kelvinUp, 0, PeripheralConfig::PWM_MAX);
    }

    ButtonTaskDefinition selectableTasks[BUTTON_TASK_OPTION_COUNT] = {
        {
            ButtonTaskOption::STRIP_POWER_AND_BRIGHTNESS,
            "Strip_PowerBrightness",
            STRIP_MODE_ANY_MASK,
            {
                nullptr,
                handlePowerClick,
                nullptr,
                nullptr,
                handlePowerLongStart,
                handlePowerDuringLong,
                handlePowerLongStop,
            },
        },
        {
            ButtonTaskOption::STRIP_WHITE_KELVIN_ADJUST,
            "Strip_WhiteColor",
            STRIP_MODE_DUAL_MASK,
            {
                nullptr,
                handleAdjustClick,
                nullptr,
                nullptr,
                handleAdjustLongStart,
                handleAdjustDuringLong,
                handleAdjustLongStop,
            },
        },
        {
            ButtonTaskOption::VOLUME_MUTED,
            "VolumeMuted",
            STRIP_MODE_ANY_MASK,
            {
                nullptr,
                handleVolumeMutedClick,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
            },
        },
        {
            ButtonTaskOption::VOLUME_UP,
            "VolumeUp",
            STRIP_MODE_ANY_MASK,
            {
                nullptr,
                handleVolumeUpClick,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
            },
        },
        {
            ButtonTaskOption::VOLUME_DOWN,
            "VolumeDown",
            STRIP_MODE_ANY_MASK,
            {
                nullptr,
                handleVolumeDownClick,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
                nullptr,
            },
        },
    };

    ButtonTaskOption buttonTaskAssignments[PeripheralConfig::BUTTON_COUNT] = {
        ButtonTaskOption::STRIP_POWER_AND_BRIGHTNESS,
        ButtonTaskOption::STRIP_WHITE_KELVIN_ADJUST,
        ButtonTaskOption::VOLUME_MUTED,
        ButtonTaskOption::VOLUME_UP,
        ButtonTaskOption::VOLUME_DOWN,
    };

    const ButtonTaskDefinition *definitionFor(ButtonTaskOption option)
    {
        for (uint8_t index = 0; index < BUTTON_TASK_OPTION_COUNT; ++index)
        {
            if (selectableTasks[index].option == option)
            {
                return &selectableTasks[index];
            }
        }

        return &selectableTasks[0];
    }

    bool isTaskAvailable(ButtonTaskOption option)
    {
        const ButtonTaskDefinition *definition = definitionFor(option);
        const uint8_t modeBit = 1 << static_cast<uint8_t>(State.stripMode);
        return (definition->stripModeMask & modeBit) != 0;
    }

    ButtonTask *taskFor(ButtonTaskOption option)
    {
        const ButtonTaskDefinition *definition = definitionFor(option);
        if (!isTaskAvailable(option))
        {
            return nullptr;
        }

        return const_cast<ButtonTask *>(&definition->task);
    }

    PeripheralButton buttons[PeripheralConfig::BUTTON_COUNT] = {
        PeripheralButton(0, PeripheralConfig::BUTTON_PINS[0], &sharedTiming, nullptr),
        PeripheralButton(1, PeripheralConfig::BUTTON_PINS[1], &sharedTiming, nullptr),
        PeripheralButton(2, PeripheralConfig::BUTTON_PINS[2], &sharedTiming, nullptr),
        PeripheralButton(3, PeripheralConfig::BUTTON_PINS[3], &sharedTiming, nullptr),
        PeripheralButton(4, PeripheralConfig::BUTTON_PINS[4], &sharedTiming, nullptr),
    };

    void syncTimingFromState()
    {
        sharedTiming.debounceMs = State.debounceMs;
        sharedTiming.clickMs = State.clickMs;
        sharedTiming.tickMs = 0;
        sharedTiming.longMs = State.longMs;
        sharedTiming.shortMs = 0;
        sharedTiming.idleMs = 0;
        sharedTiming.longPressIntervalMs = State.repeatMs;
    }

    void applyButtonTasks()
    {
        for (uint8_t index = 0; index < PeripheralConfig::BUTTON_COUNT; ++index)
        {
            buttons[index].setTask(taskFor(buttonTaskAssignments[index]));
        }
    }
}

PeripheralButton::PeripheralButton(uint8_t index, uint8_t pin, ButtonTimingConfig *timing, ButtonTask *task)
    : index_(index),
      pin_(pin),
      timing_(timing),
      task_(task),
      button_(pin, false, false)
{
}

void PeripheralButton::begin()
{
    button_.setup(pin_, INPUT, false);
    stableDown_ = isDown();
    lastRawDown_ = stableDown_;
    rawChangedAtMs_ = millis();
    bindTask();
    applyTiming();
}

void PeripheralButton::setTask(ButtonTask *task)
{
    if (task_ == task)
    {
        return;
    }

    task_ = task;
    bindTask();
}

void PeripheralButton::update()
{
    if (timingChanged())
    {
        applyTiming();
    }

    button_.tick();
    updateEdgeState();
}

bool PeripheralButton::isDown() const
{
    return digitalRead(pin_) == HIGH;
}

void PeripheralButton::bindTask()
{
    button_.attachPress(task_ != nullptr ? task_->press : nullptr);
    button_.attachClick(task_ != nullptr ? task_->click : nullptr);
    button_.attachDoubleClick(task_ != nullptr ? task_->doubleClick : nullptr);
    button_.attachMultiClick(task_ != nullptr ? task_->multiClick : nullptr);
    button_.attachLongPressStart(task_ != nullptr ? task_->longPressStart : nullptr);
    button_.attachDuringLongPress(task_ != nullptr ? task_->duringLongPress : nullptr);
    button_.attachLongPressStop(task_ != nullptr ? task_->longPressStop : nullptr);
}

void PeripheralButton::updateEdgeState()
{
    const bool rawDown = isDown();
    const uint32_t now = millis();

    if (rawDown != lastRawDown_)
    {
        lastRawDown_ = rawDown;
        rawChangedAtMs_ = now;
    }

    const uint16_t debounceMs = timing_ != nullptr ? timing_->debounceMs : PeripheralConfig::DEFAULT_DEBOUNCE_MS;
    if (now - rawChangedAtMs_ < debounceMs || rawDown == stableDown_)
    {
        return;
    }

    stableDown_ = rawDown;
    markButtonEdge(index_, stableDown_ ? ButtonEdge::Pressed : ButtonEdge::Released);
}

void PeripheralButton::applyTiming()
{
    if (timing_ == nullptr)
    {
        return;
    }

    button_.setDebounceMs(timing_->debounceMs);
    button_.setClickMs(timing_->clickMs);
    button_.setPressMs(timing_->longMs);
    button_.setLongPressIntervalMs(timing_->longPressIntervalMs);
    if (timing_->idleMs > 0)
    {
        button_.setIdleMs(timing_->idleMs);
    }
    appliedTiming_ = *timing_;
}

bool PeripheralButton::timingChanged() const
{
    if (timing_ == nullptr)
    {
        return false;
    }

    return timing_->debounceMs != appliedTiming_.debounceMs ||
           timing_->clickMs != appliedTiming_.clickMs ||
           timing_->tickMs != appliedTiming_.tickMs ||
           timing_->longMs != appliedTiming_.longMs ||
           timing_->shortMs != appliedTiming_.shortMs ||
           timing_->idleMs != appliedTiming_.idleMs ||
           timing_->longPressIntervalMs != appliedTiming_.longPressIntervalMs;
}

namespace ButtonControls
{
    void begin()
    {
        syncTimingFromState();
        for (uint8_t index = 0; index < PeripheralConfig::BUTTON_COUNT; ++index)
        {
            buttons[index].begin();
        }
        applyButtonTasks();
    }

    void update()
    {
        syncTimingFromState();
        applyButtonTasks();
        for (uint8_t index = 0; index < PeripheralConfig::BUTTON_COUNT; ++index)
        {
            buttons[index].update();
        }
        State.powerButtonDown = State.buttonDown[0];
        State.adjustButtonDown = State.buttonDown[1];
    }

    void applyTiming()
    {
        syncTimingFromState();
        for (uint8_t index = 0; index < PeripheralConfig::BUTTON_COUNT; ++index)
        {
            buttons[index].applyTiming();
        }
    }

    bool setButtonTask(uint8_t buttonIndex, ButtonTaskOption option)
    {
        if (buttonIndex >= PeripheralConfig::BUTTON_COUNT)
        {
            return false;
        }

        if (!taskAvailable(option))
        {
            return false;
        }

        buttonTaskAssignments[buttonIndex] = option;
        buttons[buttonIndex].setTask(taskFor(option));
        return true;
    }

    ButtonTaskOption buttonTask(uint8_t buttonIndex)
    {
        if (buttonIndex >= PeripheralConfig::BUTTON_COUNT)
        {
            return ButtonTaskOption::STRIP_POWER_AND_BRIGHTNESS;
        }

        return buttonTaskAssignments[buttonIndex];
    }

    bool taskAvailable(ButtonTaskOption option)
    {
        return isTaskAvailable(option);
    }

    const ButtonTaskDefinition *taskDefinitions()
    {
        return selectableTasks;
    }

    uint8_t taskDefinitionCount()
    {
        return BUTTON_TASK_OPTION_COUNT;
    }
}
