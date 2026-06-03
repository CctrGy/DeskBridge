#pragma once

#include <stdint.h>

#include "input/PanelControls.h"

class Controls
{
public:
    using Event = PanelControls::Event;

    void begin()
    {
        PanelControls::begin();
    }

    void update()
    {
        PanelControls::update();
    }

    int16_t rotation()
    {
        return PanelControls::consumeEncoderDelta();
    }

    Event event()
    {
        return PanelControls::consumeEvent();
    }
};
