#pragma once

#include <Arduino.h>
#include <OneButton.h>

struct ButtonTask
{
    using TaskFn = void (*)();

    TaskFn press = nullptr;
    TaskFn click = nullptr;
    TaskFn doubleClick = nullptr;
    TaskFn multiClick = nullptr;
    TaskFn longPressStart = nullptr;
    TaskFn duringLongPress = nullptr;
    TaskFn longPressStop = nullptr;
};

enum class ButtonTaskOption : uint8_t
{
    STRIP_POWER_AND_BRIGHTNESS = 0,
    STRIP_WHITE_KELVIN_ADJUST = 1,
    VOLUME_MUTED = 2,
    VOLUME_UP = 3,
    VOLUME_DOWN = 4,
};

constexpr uint8_t BUTTON_TASK_OPTION_COUNT = 5;

struct ButtonTaskDefinition
{
    ButtonTaskOption option;
    const char *name;
    uint8_t stripModeMask;
    ButtonTask task;
};

struct ButtonTimingConfig
{
    uint16_t debounceMs = 0;
    uint16_t clickMs = 0;
    uint16_t tickMs = 0;
    uint16_t longMs = 0;
    uint16_t shortMs = 0;
    uint16_t idleMs = 0;
    uint16_t longPressIntervalMs = 0;
};

class PeripheralButton
{
public:
    PeripheralButton(uint8_t index, uint8_t pin, ButtonTimingConfig *timing, ButtonTask *task);

    void begin();
    void update();
    void setTask(ButtonTask *task);
    bool isDown() const;
    void applyTiming();
    bool timingChanged() const;

private:
    void bindTask();
    void updateEdgeState();

    uint8_t index_;
    uint8_t pin_;
    ButtonTimingConfig *timing_;
    ButtonTask *task_;
    OneButton button_;
    ButtonTimingConfig appliedTiming_;
    bool stableDown_ = false;
    bool lastRawDown_ = false;
    uint32_t rawChangedAtMs_ = 0;
};

namespace ButtonControls
{
    void begin();
    void update();
    void applyTiming();
    bool setButtonTask(uint8_t buttonIndex, ButtonTaskOption option);
    ButtonTaskOption buttonTask(uint8_t buttonIndex);
    bool taskAvailable(ButtonTaskOption option);
    const ButtonTaskDefinition *taskDefinitions();
    uint8_t taskDefinitionCount();
}
