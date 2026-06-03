#include "bus/Aht21Sensor.h"

namespace
{
    constexpr uint8_t softResetCommand = 0xBA;
    constexpr uint8_t initCommand = 0xBE;
    constexpr uint8_t triggerMeasurementCommand = 0xAC;
    constexpr uint8_t statusCalibratedMask = 0x08;
    constexpr uint8_t statusBusyMask = 0x80;
}

bool Aht21Sensor::begin(TwoWire &wire, uint8_t address)
{
    wire_ = &wire;
    address_ = address;

    delay(40);
    writeCommand(softResetCommand);
    delay(20);

    if (!writeCommand(initCommand, 0x08, 0x00))
    {
        return false;
    }

    delay(10);

    uint8_t status = 0;
    return readStatus(status) && (status & statusCalibratedMask) != 0;
}

bool Aht21Sensor::read(float &temperatureC, float &humidity)
{
    if (!wire_)
    {
        return false;
    }

    if (!writeCommand(triggerMeasurementCommand, 0x33, 0x00))
    {
        return false;
    }

    delay(80);

    uint8_t buffer[6] = {};
    const uint8_t requested = wire_->requestFrom(address_, static_cast<uint8_t>(sizeof(buffer)));
    if (requested != sizeof(buffer))
    {
        return false;
    }

    for (uint8_t index = 0; index < sizeof(buffer); ++index)
    {
        buffer[index] = wire_->read();
    }

    if ((buffer[0] & statusBusyMask) != 0)
    {
        return false;
    }

    const uint32_t rawHumidity = (static_cast<uint32_t>(buffer[1]) << 12) |
                                 (static_cast<uint32_t>(buffer[2]) << 4) |
                                 (static_cast<uint32_t>(buffer[3]) >> 4);
    const uint32_t rawTemperature = ((static_cast<uint32_t>(buffer[3]) & 0x0F) << 16) |
                                    (static_cast<uint32_t>(buffer[4]) << 8) |
                                    static_cast<uint32_t>(buffer[5]);

    humidity = (static_cast<float>(rawHumidity) * 100.0f) / 1048576.0f;
    temperatureC = (static_cast<float>(rawTemperature) * 200.0f) / 1048576.0f - 50.0f;
    return true;
}

bool Aht21Sensor::writeCommand(uint8_t command)
{
    if (!wire_)
    {
        return false;
    }

    wire_->beginTransmission(address_);
    wire_->write(command);
    return wire_->endTransmission() == 0;
}

bool Aht21Sensor::writeCommand(uint8_t command, uint8_t arg0, uint8_t arg1)
{
    if (!wire_)
    {
        return false;
    }

    wire_->beginTransmission(address_);
    wire_->write(command);
    wire_->write(arg0);
    wire_->write(arg1);
    return wire_->endTransmission() == 0;
}

bool Aht21Sensor::readStatus(uint8_t &status)
{
    if (!wire_)
    {
        return false;
    }

    if (wire_->requestFrom(address_, static_cast<uint8_t>(1)) != 1)
    {
        return false;
    }

    status = wire_->read();
    return true;
}
