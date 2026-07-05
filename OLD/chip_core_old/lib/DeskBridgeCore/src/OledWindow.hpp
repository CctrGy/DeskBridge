#pragma once

#include <Arduino.h>

class OledWindow
{
public:
    OledWindow(const char *title = "")
        : title_(title)
    {
    }

    const char *title() const
    {
        return title_;
    }

    void setTitle(const char *title)
    {
        title_ = title;
    }

private:
    const char *title_;
};
