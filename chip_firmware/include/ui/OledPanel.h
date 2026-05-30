#pragma once

namespace OledPanel
{
    void begin();
    void bootBegin();
    void update();
    void showBootStatus(const char *line);
    void bootFinish();
}
