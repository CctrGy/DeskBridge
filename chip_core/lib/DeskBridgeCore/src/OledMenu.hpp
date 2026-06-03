#pragma once

#include "ui/OledPanel.h"

class OledMenu
{
public:
    void begin()
    {
        OledPanel::begin();
    }

    void update()
    {
        OledPanel::update();
    }

    void bootBegin()
    {
        OledPanel::bootBegin();
    }

    void bootStatus(const char *line)
    {
        OledPanel::showBootStatus(line);
    }

    void bootFinish()
    {
        OledPanel::bootFinish();
    }
};
