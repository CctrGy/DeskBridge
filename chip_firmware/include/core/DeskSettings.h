#pragma once

#include <stdint.h>

namespace DeskSettings
{
    struct Config
    {
        uint32_t magic;
        uint16_t version;
        uint16_t size;
        uint16_t checksum;

        uint8_t stripEnabled;
        uint8_t stripMode;
        uint16_t stripBrightness;
        uint16_t stripBrightnessMin;
        uint16_t stripBrightnessMax;
        uint16_t stripKelvinMix;
        uint16_t stripColdMin;
        uint16_t stripColdMax;
        uint16_t stripHotMin;
        uint16_t stripHotMax;
        uint16_t stripButtonClickMs;
        uint16_t stripButtonLongMs;
        uint16_t stripButtonRepeatMs;
        uint16_t stripBrightnessStep;
        uint16_t stripKelvinStep;
        uint8_t stripButtonAction0;
        uint8_t stripButtonAction1;
        uint8_t stripButtonAction2;
        uint8_t stripButtonAction3;

        uint32_t displayTimeoutMs;
        uint32_t sensorSampleIntervalMs;
        uint8_t sensorSampleCount;
        uint8_t temperatureUnit;
        uint8_t homeBoxBig;
        uint8_t homeBoxSmall;
        uint8_t homeBoxData1;
        uint8_t homeBoxData2;
        uint8_t homeBoxData3;
    };

    void begin();
    void update();
    bool loaded();
    bool storageReady();
    Config &data();
    const Config &dataConst();
    void captureStripFromRuntime();
    void resetDefaults();
    void markDirty();
    bool saveNow();
}
