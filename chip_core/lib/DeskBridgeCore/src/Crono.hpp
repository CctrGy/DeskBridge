#pragma once

#include <Arduino.h>
#include <stdint.h>

class Crono
{
public:
    explicit Crono(uint32_t intervalMs = 1000, bool startNow = true)
        : intervalMs_(intervalMs), lastMs_(startNow ? millis() : 0)
    {
    }

    void setInterval(uint32_t intervalMs)
    {
        intervalMs_ = intervalMs;
    }

    uint32_t interval() const
    {
        return intervalMs_;
    }

    void reset()
    {
        lastMs_ = millis();
    }

    bool elapsed()
    {
        const uint32_t now = millis();
        if (now - lastMs_ < intervalMs_)
        {
            return false;
        }

        lastMs_ = now;
        return true;
    }

    bool elapsed(uint32_t intervalMs)
    {
        setInterval(intervalMs);
        return elapsed();
    }

    uint32_t age() const
    {
        return millis() - lastMs_;
    }

private:
    uint32_t intervalMs_;
    uint32_t lastMs_;
};
