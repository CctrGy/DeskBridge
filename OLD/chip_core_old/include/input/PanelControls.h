#pragma once

#include <stdint.h>

namespace PanelControls
{
    enum class Event : uint8_t
    {
        None,
        EncoderClick,
        EscapeClick,
    };

    void begin();
    void update();
    int16_t consumeEncoderDelta();
    Event consumeEvent();
}
