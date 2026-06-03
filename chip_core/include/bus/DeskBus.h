#pragma once

#include <Arduino.h>
#include <RTClib.h>

namespace DeskBus
{
    enum class Device : uint8_t
    {
        DS3231,
        AT24C32,
        Keypad,
        ENS160,
        AHT21,
        VEML7700,
        SCD41,
        SHT21,
        Count,
    };

    struct Measurements
    {
        float temperature = NAN;
        float humidity = NAN;
        float shtTemperature = NAN;
        float shtHumidity = NAN;
        float lux = NAN;
        int16_t co2Ppm = -1;
        int16_t ensEco2Ppm = -1;
        int16_t tvocPpb = -1;
        uint8_t airQualityIndex = 0;
    };

    void begin();
    void update();
    void pauseSampling(uint32_t durationMs);

    bool ready(Device device);
    const char *name(Device device);
    const char *addressText(Device device);
    bool devicePresent(uint8_t address);
    uint8_t scan(uint8_t *addresses, uint8_t capacity);
    uint8_t scanSystem(uint8_t *addresses, uint8_t capacity);
    uint8_t scanSensors(uint8_t *addresses, uint8_t capacity);
    bool systemHealthy();

    const Measurements &measurements();

    DateTime now();
    bool setRtc(const DateTime &dateTime);
    bool rtcLostPower();

    uint8_t eepromRead(uint32_t address);
    bool eepromWrite(uint32_t address, uint8_t value);
}
