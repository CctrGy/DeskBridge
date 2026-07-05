#include "bus/DeskBus.h"

#include <Adafruit_VEML7700.h>
#include <ScioSense_ENS160.h>
#include <SensirionI2cScd4x.h>
#include <SHTSensor.h>
#include <SparkFun_External_EEPROM.h>
#include <Wire.h>

#include "bus/Aht21Sensor.h"
#include "bus/BoardI2C.h"
#include "bus/KeypadPeripheral.h"
#include "config/DeskConfig.h"
#include "config/pins_config.h"
#include "core/DeskSettings.h"

namespace
{
    constexpr uint32_t climateIntervalMs = SENSOR_CLIMATE_INTERVAL_MS_DEFAULT;
    constexpr uint32_t airQualityIntervalMs = SENSOR_AIR_QUALITY_INTERVAL_MS_DEFAULT;
    constexpr uint32_t lightIntervalMs = SENSOR_LIGHT_INTERVAL_MS_DEFAULT;
    constexpr uint32_t shtIntervalMs = SENSOR_SHT_INTERVAL_MS_DEFAULT;
    constexpr uint32_t co2PollIntervalMs = SENSOR_CO2_POLL_INTERVAL_MS_DEFAULT;
    constexpr uint8_t sensorSlotCount = SENSOR_SLOT_COUNT_DEFAULT;
    constexpr uint32_t minimumSensorAttemptSpacingMs = SENSOR_ATTEMPT_SPACING_MS_MIN_DEFAULT;

    RTC_DS3231 rtc;
    ExternalEEPROM eeprom;
    uint8_t eepromAddress = 0x50;
    ScioSense_ENS160 ens160Address0(&BoardI2C::sensorWire(), ENS160_I2CADDR_0);
    ScioSense_ENS160 ens160Address1(&BoardI2C::sensorWire(), ENS160_I2CADDR_1);
    ScioSense_ENS160 *ens160 = &ens160Address0;
    Aht21Sensor aht21;
    Adafruit_VEML7700 veml7700;
    SensirionI2cScd4x scd41;
    SHTSensor sht21(SHTSensor::SHT2X);

    bool initialized = false;
    bool readyFlags[static_cast<uint8_t>(DeskBus::Device::Count)] = {};
    bool rtcPowerLost = false;
    DeskBus::Measurements cached;

    uint32_t lastClimateMs = 0;
    uint32_t lastAirQualityMs = 0;
    uint32_t lastLightMs = 0;
    uint32_t lastShtMs = 0;
    uint32_t lastCo2PollMs = 0;
    uint32_t lastSensorAttemptMs = 0;
    uint32_t lastSystemHealthMs = 0;
    uint32_t samplingPausedUntilMs = 0;
    uint8_t updateSlot = 0;

    struct FloatPairAverage
    {
        float first = 0.0f;
        float second = 0.0f;
        uint8_t count = 0;
    };

    struct FloatAverage
    {
        float value = 0.0f;
        uint8_t count = 0;
    };

    struct AirAverage
    {
        uint32_t eco2 = 0;
        uint32_t tvoc = 0;
        uint16_t aqi = 0;
        uint8_t count = 0;
    };

    struct Co2Average
    {
        uint32_t co2 = 0;
        uint8_t count = 0;
    };

    FloatPairAverage ahtAverage;
    FloatPairAverage shtAverage;
    FloatPairAverage scdClimateAverage;
    FloatAverage luxAverage;
    AirAverage airAverage;
    Co2Average co2Average;

    uint8_t indexOf(DeskBus::Device device)
    {
        return static_cast<uint8_t>(device);
    }

    bool &readyFlag(DeskBus::Device device)
    {
        return readyFlags[indexOf(device)];
    }

    float validTemperature(float value)
    {
        return isnan(value) || value < SENSOR_TEMP_MIN_C_DEFAULT || value > SENSOR_TEMP_MAX_C_DEFAULT ? NAN : value;
    }

    float validHumidity(float value)
    {
        return isnan(value) || value < SENSOR_HUMIDITY_MIN_DEFAULT || value > SENSOR_HUMIDITY_MAX_DEFAULT ? NAN : value;
    }

    uint8_t targetSampleCount()
    {
        return DeskSettings::loaded() ? constrain(DeskSettings::data().sensorSampleCount, SENSOR_SAMPLE_COUNT_MIN_DEFAULT, SENSOR_SAMPLE_COUNT_MAX_DEFAULT) : SENSOR_SAMPLE_COUNT_DEFAULT;
    }

    uint32_t sensorAttemptSpacingMs()
    {
        const uint32_t intervalMs = DeskSettings::loaded() ? DeskSettings::data().sensorSampleIntervalMs : SENSOR_SAMPLE_INTERVAL_MS_DEFAULT;
        const uint32_t attemptCount = static_cast<uint32_t>(sensorSlotCount) * targetSampleCount();
        const uint32_t spacing = intervalMs / (attemptCount == 0 ? 1 : attemptCount);
        return spacing < minimumSensorAttemptSpacingMs ? minimumSensorAttemptSpacingMs : spacing;
    }

    void addPairSample(FloatPairAverage &average, float first, float second, float &publishedFirst, float &publishedSecond)
    {
        if (isnan(first) || isnan(second))
        {
            return;
        }

        average.first += first;
        average.second += second;
        ++average.count;

        const uint8_t target = targetSampleCount();
        if (average.count >= target)
        {
            publishedFirst = average.first / average.count;
            publishedSecond = average.second / average.count;
            average = {};
        }
    }

    void updateAht21(uint32_t nowMs)
    {
        if (!readyFlag(DeskBus::Device::AHT21) || nowMs - lastClimateMs < climateIntervalMs)
        {
            return;
        }

        lastClimateMs = nowMs;
        float temperature = NAN;
        float humidity = NAN;
        if (aht21.read(temperature, humidity))
        {
            addPairSample(ahtAverage,
                          validTemperature(temperature),
                          validHumidity(humidity),
                          cached.temperature,
                          cached.humidity);
        }
    }

    void updateEns160(uint32_t nowMs)
    {
        if (!readyFlag(DeskBus::Device::ENS160) || nowMs - lastAirQualityMs < airQualityIntervalMs)
        {
            return;
        }

        lastAirQualityMs = nowMs;
        if (!isnan(cached.temperature) && !isnan(cached.humidity))
        {
            ens160->set_envdata(cached.temperature, cached.humidity);
        }

        if (ens160->measure())
        {
            airAverage.aqi += ens160->getAQI();
            airAverage.eco2 += ens160->geteCO2();
            airAverage.tvoc += ens160->getTVOC();
            ++airAverage.count;

            const uint8_t target = targetSampleCount();
            if (airAverage.count >= target)
            {
                cached.airQualityIndex = static_cast<uint8_t>(airAverage.aqi / airAverage.count);
                cached.ensEco2Ppm = static_cast<int16_t>(airAverage.eco2 / airAverage.count);
                cached.tvocPpb = static_cast<int16_t>(airAverage.tvoc / airAverage.count);
                airAverage = {};
            }
        }
    }

    void updateVeml7700(uint32_t nowMs)
    {
        if (!readyFlag(DeskBus::Device::VEML7700) || nowMs - lastLightMs < lightIntervalMs)
        {
            return;
        }

        lastLightMs = nowMs;
        const float lux = veml7700.readLux(VEML_LUX_AUTO);
        if (!isnan(lux) && lux >= 0.0f)
        {
            luxAverage.value += lux;
            ++luxAverage.count;

            const uint8_t target = targetSampleCount();
            if (luxAverage.count >= target)
            {
                cached.lux = luxAverage.value / luxAverage.count;
                luxAverage = {};
            }
        }
    }

    void updateScd41(uint32_t nowMs)
    {
        if (!readyFlag(DeskBus::Device::SCD41) || nowMs - lastCo2PollMs < co2PollIntervalMs)
        {
            return;
        }

        lastCo2PollMs = nowMs;

        bool dataReady = false;
        if (scd41.getDataReadyStatus(dataReady) != 0 || !dataReady)
        {
            return;
        }

        uint16_t co2 = 0;
        float temperature = 0.0f;
        float humidity = 0.0f;
        if (scd41.readMeasurement(co2, temperature, humidity) == 0)
        {
            co2Average.co2 += co2;
            ++co2Average.count;
            if (!readyFlag(DeskBus::Device::AHT21))
            {
                addPairSample(scdClimateAverage, validTemperature(temperature), validHumidity(humidity), cached.temperature, cached.humidity);
            }

            const uint8_t target = targetSampleCount();
            if (co2Average.count >= target)
            {
                cached.co2Ppm = static_cast<int16_t>(co2Average.co2 / co2Average.count);
                co2Average = {};
            }
        }
    }

    void updateSht21(uint32_t nowMs)
    {
        if (!readyFlag(DeskBus::Device::SHT21) || nowMs - lastShtMs < shtIntervalMs)
        {
            return;
        }

        lastShtMs = nowMs;
        if (sht21.readSample())
        {
            addPairSample(shtAverage,
                          validTemperature(sht21.getTemperature()),
                          validHumidity(sht21.getHumidity()),
                          cached.shtTemperature,
                          cached.shtHumidity);
        }
    }

    bool beginExternalEeprom()
    {
        for (uint8_t address = EEPROM_I2C_ADDRESS_MIN_DEFAULT; address <= EEPROM_I2C_ADDRESS_MAX_DEFAULT; ++address)
        {
            if (address == ENS160_I2CADDR_0 || address == ENS160_I2CADDR_1)
            {
                continue;
            }

            if (!BoardI2C::devicePresent(BoardI2C::Bus::System, address))
            {
                continue;
            }

            if (eeprom.begin(address, BoardI2C::systemWire()))
            {
                eepromAddress = address;
                return true;
            }
        }

        return false;
    }

    void configureExternalEeprom()
    {
        eeprom.setMemorySizeBytes(EEPROM_SIZE_BYTES_DEFAULT);
        eeprom.setAddressBytes(EEPROM_ADDRESS_BYTES_DEFAULT);
        eeprom.setPageSizeBytes(EEPROM_PAGE_SIZE_BYTES_DEFAULT);
        eeprom.setWriteTimeMs(EEPROM_WRITE_TIME_MS_DEFAULT);
    }

    void appendAddressIfPresent(BoardI2C::Bus bus, uint8_t address, uint8_t *addresses, uint8_t capacity, uint8_t &found)
    {
        if (found >= capacity || !BoardI2C::devicePresent(bus, address))
        {
            return;
        }

        for (uint8_t index = 0; index < found; ++index)
        {
            if (addresses[index] == address)
            {
                return;
            }
        }

        addresses[found++] = address;
    }

    void updateSystemI2cHealth(uint32_t nowMs)
    {
        if (nowMs - lastSystemHealthMs < SYSTEM_I2C_HEALTH_INTERVAL_MS_DEFAULT)
        {
            return;
        }

        lastSystemHealthMs = nowMs;

        const bool rtcPresent = BoardI2C::devicePresent(BoardI2C::Bus::System, 0x68);
        if (!rtcPresent)
        {
            readyFlag(DeskBus::Device::DS3231) = false;
        }
        else if (!readyFlag(DeskBus::Device::DS3231))
        {
            readyFlag(DeskBus::Device::DS3231) = rtc.begin(&BoardI2C::systemWire());
        }

        if (readyFlag(DeskBus::Device::AT24C32))
        {
            readyFlag(DeskBus::Device::AT24C32) = BoardI2C::devicePresent(BoardI2C::Bus::System, eepromAddress);
        }

        if (!readyFlag(DeskBus::Device::AT24C32))
        {
            readyFlag(DeskBus::Device::AT24C32) = beginExternalEeprom();
            if (readyFlag(DeskBus::Device::AT24C32))
            {
                configureExternalEeprom();
            }
        }

        KeypadPeripheral::update();
        readyFlag(DeskBus::Device::Keypad) = KeypadPeripheral::present();
    }
}

namespace DeskBus
{
    void begin()
    {
        if (initialized)
        {
            return;
        }

        initialized = true;
        KeypadPeripheral::prepareResetHold();
        BoardI2C::begin();

        readyFlag(Device::DS3231) = rtc.begin(&BoardI2C::systemWire());
        if (readyFlag(Device::DS3231))
        {
            rtcPowerLost = rtc.lostPower();
            if (rtcPowerLost)
            {
                rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
            }
        }

        readyFlag(Device::AT24C32) = beginExternalEeprom();
        if (readyFlag(Device::AT24C32))
        {
            configureExternalEeprom();
        }

        KeypadPeripheral::begin();
        readyFlag(Device::Keypad) = KeypadPeripheral::present();

        if (BoardI2C::devicePresent(BoardI2C::Bus::Sensors, ENS160_I2CADDR_0))
        {
            ens160 = &ens160Address0;
        }
        else if (BoardI2C::devicePresent(BoardI2C::Bus::Sensors, ENS160_I2CADDR_1))
        {
            ens160 = &ens160Address1;
        }

        readyFlag(Device::ENS160) = ens160->begin();
        if (readyFlag(Device::ENS160))
        {
            readyFlag(Device::ENS160) = ens160->setMode(ENS160_OPMODE_STD);
        }

        readyFlag(Device::AHT21) = aht21.begin(BoardI2C::sensorWire());

        readyFlag(Device::VEML7700) = veml7700.begin(&BoardI2C::sensorWire());
        if (readyFlag(Device::VEML7700))
        {
            veml7700.setGain(VEML7700_GAIN_1);
            veml7700.setIntegrationTime(VEML7700_IT_100MS);
        }

        scd41.begin(BoardI2C::sensorWire(), SCD41_I2C_ADDR_62);
        readyFlag(Device::SCD41) = BoardI2C::devicePresent(BoardI2C::Bus::Sensors, 0x62);
        if (readyFlag(Device::SCD41))
        {
            scd41.stopPeriodicMeasurement();
            scd41.reinit();
            readyFlag(Device::SCD41) = scd41.startPeriodicMeasurement() == 0;
        }

        readyFlag(Device::SHT21) = sht21.init(BoardI2C::sensorWire());
    }

    void update()
    {
        if (!initialized)
        {
            begin();
        }

        const uint32_t nowMs = millis();
        updateSystemI2cHealth(nowMs);

        if (static_cast<int32_t>(nowMs - samplingPausedUntilMs) < 0)
        {
            lastSensorAttemptMs = nowMs;
            return;
        }

        if (nowMs - lastSensorAttemptMs < sensorAttemptSpacingMs())
        {
            return;
        }

        lastSensorAttemptMs = nowMs;

        switch (updateSlot)
        {
            case 0:
                updateAht21(nowMs);
                break;
            case 1:
                updateEns160(nowMs);
                break;
            case 2:
                updateVeml7700(nowMs);
                break;
            case 3:
                updateScd41(nowMs);
                break;
            case 4:
                updateSht21(nowMs);
                break;
            default:
                break;
        }

        updateSlot = (updateSlot + 1) % sensorSlotCount;
    }

    void pauseSampling(uint32_t durationMs)
    {
        samplingPausedUntilMs = millis() + durationMs;
    }

    bool ready(Device device)
    {
        return readyFlags[indexOf(device)];
    }

    const char *name(Device device)
    {
        switch (device)
        {
            case Device::DS3231:
                return "DS3231";
            case Device::AT24C32:
                return "AT24C32";
            case Device::Keypad:
                return "KEYPAD";
            case Device::ENS160:
                return "ENS160";
            case Device::AHT21:
                return "AHT21";
            case Device::VEML7700:
                return "VEML7700";
            case Device::SCD41:
                return "SCD41";
            case Device::SHT21:
                return "SHT21";
            default:
                return "--";
        }
    }

    const char *addressText(Device device)
    {
        switch (device)
        {
            case Device::DS3231:
                return "0x68";
            case Device::AT24C32:
            {
                static char address[5] = {};
                snprintf(address, sizeof(address), "0x%02X", eepromAddress);
                return address;
            }
            case Device::Keypad:
                return "UART";
            case Device::ENS160:
                return "0x52/0x53";
            case Device::AHT21:
                return "0x38";
            case Device::VEML7700:
                return "0x10";
            case Device::SCD41:
                return "0x62";
            case Device::SHT21:
                return "0x40";
            default:
                return "--";
        }
    }

    bool devicePresent(uint8_t address)
    {
        return BoardI2C::devicePresent(BoardI2C::Bus::System, address) ||
               BoardI2C::devicePresent(BoardI2C::Bus::Sensors, address);
    }

    uint8_t scan(uint8_t *addresses, uint8_t capacity)
    {
        uint8_t found = BoardI2C::scan(BoardI2C::Bus::System, addresses, capacity);
        uint8_t sensorAddresses[16] = {};
        const uint8_t sensorFound = BoardI2C::scan(BoardI2C::Bus::Sensors, sensorAddresses, sizeof(sensorAddresses));

        for (uint8_t sensorIndex = 0; sensorIndex < sensorFound && found < capacity; ++sensorIndex)
        {
            bool duplicate = false;
            for (uint8_t existingIndex = 0; existingIndex < found; ++existingIndex)
            {
                if (addresses[existingIndex] == sensorAddresses[sensorIndex])
                {
                    duplicate = true;
                    break;
                }
            }

            if (!duplicate)
            {
                addresses[found++] = sensorAddresses[sensorIndex];
            }
        }

        return found;
    }

    uint8_t scanSystem(uint8_t *addresses, uint8_t capacity)
    {
        uint8_t found = 0;
        appendAddressIfPresent(BoardI2C::Bus::System, 0x68, addresses, capacity, found);

        for (uint8_t address = EEPROM_I2C_ADDRESS_MIN_DEFAULT; address <= EEPROM_I2C_ADDRESS_MAX_DEFAULT; ++address)
        {
            appendAddressIfPresent(BoardI2C::Bus::System, address, addresses, capacity, found);
        }

        return found;
    }

    uint8_t scanSensors(uint8_t *addresses, uint8_t capacity)
    {
        uint8_t found = 0;
        appendAddressIfPresent(BoardI2C::Bus::Sensors, ENS160_I2CADDR_0, addresses, capacity, found);
        appendAddressIfPresent(BoardI2C::Bus::Sensors, ENS160_I2CADDR_1, addresses, capacity, found);
        appendAddressIfPresent(BoardI2C::Bus::Sensors, 0x38, addresses, capacity, found);
        appendAddressIfPresent(BoardI2C::Bus::Sensors, 0x10, addresses, capacity, found);
        appendAddressIfPresent(BoardI2C::Bus::Sensors, 0x62, addresses, capacity, found);
        appendAddressIfPresent(BoardI2C::Bus::Sensors, 0x40, addresses, capacity, found);
        return found;
    }

    bool systemHealthy()
    {
        return ready(Device::DS3231) && ready(Device::AT24C32) && ready(Device::Keypad);
    }

    const Measurements &measurements()
    {
        return cached;
    }

    DateTime now()
    {
        return ready(Device::DS3231) ? rtc.now() : DateTime(F(__DATE__), F(__TIME__));
    }

    bool setRtc(const DateTime &dateTime)
    {
        if (!ready(Device::DS3231))
        {
            return false;
        }

        rtc.adjust(dateTime);
        rtcPowerLost = false;
        return true;
    }

    bool rtcLostPower()
    {
        return rtcPowerLost;
    }

    uint8_t eepromRead(uint32_t address)
    {
        return ready(Device::AT24C32) ? eeprom.read(address) : 0xFF;
    }

    bool eepromWrite(uint32_t address, uint8_t value)
    {
        return ready(Device::AT24C32) && eeprom.write(address, value) == 0;
    }
}
