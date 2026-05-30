#include "core/DeskSettings.h"

#include <Arduino.h>
#include <stddef.h>

#include "bus/DeskBus.h"
#include "config/DeskConfig.h"
#include "core/EStore.h"
#include "light/StripLight.h"

namespace
{
    constexpr uint32_t settingsAddress = SETTINGS_ADDRESS_DEFAULT;
    constexpr uint32_t settingsMagic = SETTINGS_MAGIC_DEFAULT;
    constexpr uint16_t settingsVersion = SETTINGS_VERSION_DEFAULT;
    constexpr uint32_t saveDelayMs = SETTINGS_SAVE_DELAY_MS_DEFAULT;

    DeskSettings::Config settings;
    DeskStorage::EStore<DeskSettings::Config> store(settingsAddress, &settings, DeskBus::eepromRead, DeskBus::eepromWrite);

    bool hasLoaded = false;
    bool storageAvailable = false;
    bool dirty = false;
    uint32_t dirtySinceMs = 0;

    uint16_t calculateChecksum(const DeskSettings::Config &value)
    {
        const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&value);
        uint16_t checksum = SETTINGS_CHECKSUM_SEED_DEFAULT;
        const size_t checksumOffset = offsetof(DeskSettings::Config, checksum);

        for (size_t index = 0; index < sizeof(DeskSettings::Config); ++index)
        {
            if (index == checksumOffset || index == checksumOffset + 1)
            {
                continue;
            }

            checksum = static_cast<uint16_t>((checksum << 5) ^ (checksum >> 11) ^ bytes[index]);
        }

        return checksum;
    }

    void setDefaults()
    {
        settings = {};
        settings.magic = settingsMagic;
        settings.version = settingsVersion;
        settings.size = sizeof(DeskSettings::Config);
        settings.stripEnabled = STRIP_ENABLED_DEFAULT;
        settings.stripMode = STRIP_MODE_DEFAULT;
        settings.stripBrightness = STRIP_BRIGHTNESS_DEFAULT;
        settings.stripBrightnessMin = STRIP_BRIGHTNESS_MIN_DEFAULT;
        settings.stripBrightnessMax = STRIP_BRIGHTNESS_MAX_DEFAULT;
        settings.stripKelvinMix = STRIP_KELVIN_MIX_DEFAULT;
        settings.stripColdMin = STRIP_COLD_MIN_DEFAULT;
        settings.stripColdMax = STRIP_COLD_MAX_DEFAULT;
        settings.stripHotMin = STRIP_HOT_MIN_DEFAULT;
        settings.stripHotMax = STRIP_HOT_MAX_DEFAULT;
        settings.stripButtonClickMs = STRIP_BUTTON_CLICK_MS_DEFAULT;
        settings.stripButtonLongMs = STRIP_BUTTON_LONG_MS_DEFAULT;
        settings.stripButtonRepeatMs = STRIP_BUTTON_REPEAT_MS_DEFAULT;
        settings.stripBrightnessStep = STRIP_BRIGHTNESS_STEP_DEFAULT;
        settings.stripKelvinStep = STRIP_KELVIN_STEP_DEFAULT;
        settings.displayTimeoutMs = DISPLAY_TIMEOUT_MS_DEFAULT;
        settings.sensorSampleIntervalMs = SENSOR_SAMPLE_INTERVAL_MS_DEFAULT;
        settings.sensorSampleCount = SENSOR_SAMPLE_COUNT_DEFAULT;
        settings.temperatureUnit = TEMPERATURE_UNIT_DEFAULT;
        settings.homeBoxBig = HOME_BOX_BIG_DEFAULT;
        settings.homeBoxSmall = HOME_BOX_SMALL_DEFAULT;
        settings.homeBoxData1 = HOME_BOX_DATA1_DEFAULT;
        settings.homeBoxData2 = HOME_BOX_DATA2_DEFAULT;
        settings.homeBoxData3 = HOME_BOX_DATA3_DEFAULT;
        settings.checksum = calculateChecksum(settings);
    }

    bool isValid()
    {
        return settings.magic == settingsMagic &&
               settings.version == settingsVersion &&
               settings.size == sizeof(DeskSettings::Config) &&
               settings.checksum == calculateChecksum(settings);
    }

    void sanitize()
    {
        settings.stripEnabled = settings.stripEnabled ? 1 : 0;
        settings.stripMode = settings.stripMode > 2 ? STRIP_MODE_DEFAULT : settings.stripMode;
        settings.stripBrightness = constrain(settings.stripBrightness, 0, StripLight::pwmMax());
        settings.stripBrightnessMin = constrain(settings.stripBrightnessMin, 0, StripLight::pwmMax());
        settings.stripBrightnessMax = constrain(settings.stripBrightnessMax, 0, StripLight::pwmMax());
        if (settings.stripBrightnessMin > settings.stripBrightnessMax)
        {
            settings.stripBrightnessMin = settings.stripBrightnessMax;
        }
        settings.stripKelvinMix = constrain(settings.stripKelvinMix, 0, StripLight::pwmMax());
        settings.stripColdMin = constrain(settings.stripColdMin, 0, StripLight::pwmMax());
        settings.stripColdMax = constrain(settings.stripColdMax, 0, StripLight::pwmMax());
        if (settings.stripColdMin > settings.stripColdMax)
        {
            settings.stripColdMin = settings.stripColdMax;
        }
        settings.stripHotMin = constrain(settings.stripHotMin, 0, StripLight::pwmMax());
        settings.stripHotMax = constrain(settings.stripHotMax, 0, StripLight::pwmMax());
        if (settings.stripHotMin > settings.stripHotMax)
        {
            settings.stripHotMin = settings.stripHotMax;
        }
        settings.stripButtonClickMs = constrain(settings.stripButtonClickMs, STRIP_BUTTON_CLICK_MS_MIN_DEFAULT, STRIP_BUTTON_CLICK_MS_MAX_DEFAULT);
        settings.stripButtonLongMs = constrain(settings.stripButtonLongMs, STRIP_BUTTON_LONG_MS_MIN_DEFAULT, STRIP_BUTTON_LONG_MS_MAX_DEFAULT);
        settings.stripButtonRepeatMs = constrain(settings.stripButtonRepeatMs, STRIP_BUTTON_REPEAT_MS_MIN_DEFAULT, STRIP_BUTTON_REPEAT_MS_MAX_DEFAULT);
        settings.stripBrightnessStep = constrain(settings.stripBrightnessStep, STRIP_STEP_MIN_DEFAULT, STRIP_STEP_MAX_DEFAULT);
        settings.stripKelvinStep = constrain(settings.stripKelvinStep, STRIP_STEP_MIN_DEFAULT, STRIP_STEP_MAX_DEFAULT);
        settings.displayTimeoutMs = constrain(settings.displayTimeoutMs, DISPLAY_TIMEOUT_MS_MIN_DEFAULT, DISPLAY_TIMEOUT_MS_MAX_DEFAULT);
        settings.sensorSampleIntervalMs = constrain(settings.sensorSampleIntervalMs, SENSOR_SAMPLE_INTERVAL_MS_MIN_DEFAULT, SENSOR_SAMPLE_INTERVAL_MS_MAX_DEFAULT);
        settings.sensorSampleCount = constrain(settings.sensorSampleCount, SENSOR_SAMPLE_COUNT_MIN_DEFAULT, SENSOR_SAMPLE_COUNT_MAX_DEFAULT);
        settings.temperatureUnit = settings.temperatureUnit > 1 ? 0 : settings.temperatureUnit;

        const uint8_t maxHomeField = HOME_FIELD_MAX_DEFAULT;
        settings.homeBoxBig = settings.homeBoxBig > maxHomeField ? 0 : settings.homeBoxBig;
        settings.homeBoxSmall = settings.homeBoxSmall > maxHomeField ? 1 : settings.homeBoxSmall;
        settings.homeBoxData1 = settings.homeBoxData1 > maxHomeField ? 2 : settings.homeBoxData1;
        settings.homeBoxData2 = settings.homeBoxData2 > maxHomeField ? 3 : settings.homeBoxData2;
        settings.homeBoxData3 = settings.homeBoxData3 > maxHomeField ? 4 : settings.homeBoxData3;
    }

    void applyToRuntime()
    {
        StripLight::setBrightnessMin(settings.stripBrightnessMin);
        StripLight::setBrightnessMax(settings.stripBrightnessMax);
        StripLight::setColdMin(settings.stripColdMin);
        StripLight::setColdMax(settings.stripColdMax);
        StripLight::setHotMin(settings.stripHotMin);
        StripLight::setHotMax(settings.stripHotMax);
        StripLight::setBrightness(settings.stripBrightness);
        StripLight::setKelvinMix(settings.stripKelvinMix);
        StripLight::setButtonClickTime(settings.stripButtonClickMs);
        StripLight::setButtonLongTime(settings.stripButtonLongMs);
        StripLight::setButtonDuringTime(settings.stripButtonRepeatMs);
        StripLight::setBrightnessStep(settings.stripBrightnessStep);
        StripLight::setKelvinStep(settings.stripKelvinStep);
        switch (settings.stripMode)
        {
            case 0:
                StripLight::setMode(StripLight::Mode::OneSimple);
                break;
            case 1:
                StripLight::setMode(StripLight::Mode::TwoSimple);
                break;
            default:
                StripLight::setMode(StripLight::Mode::Composite);
                break;
        }
        StripLight::setEnabled(settings.stripEnabled != 0);
    }

    void refreshHeaderAndChecksum()
    {
        settings.magic = settingsMagic;
        settings.version = settingsVersion;
        settings.size = sizeof(DeskSettings::Config);
        sanitize();
        settings.checksum = calculateChecksum(settings);
    }
}

namespace DeskSettings
{
    void begin()
    {
        storageAvailable = DeskBus::ready(DeskBus::Device::AT24C32);
        if (storageAvailable && store.load() && isValid())
        {
            sanitize();
        }
        else
        {
            setDefaults();
            if (storageAvailable)
            {
                store.save();
            }
        }

        applyToRuntime();
        dirty = false;
        hasLoaded = true;
    }

    void update()
    {
        if (dirty && millis() - dirtySinceMs >= saveDelayMs)
        {
            saveNow();
        }
    }

    bool loaded()
    {
        return hasLoaded;
    }

    bool storageReady()
    {
        return storageAvailable;
    }

    Config &data()
    {
        return settings;
    }

    const Config &dataConst()
    {
        return settings;
    }

    void captureStripFromRuntime()
    {
        settings.stripEnabled = StripLight::enabled() ? 1 : 0;
        switch (StripLight::mode())
        {
            case StripLight::Mode::OneSimple:
                settings.stripMode = 0;
                break;
            case StripLight::Mode::TwoSimple:
                settings.stripMode = 1;
                break;
            case StripLight::Mode::Composite:
            default:
                settings.stripMode = 2;
                break;
        }
        settings.stripBrightness = StripLight::brightness();
        settings.stripBrightnessMin = StripLight::brightnessMin();
        settings.stripBrightnessMax = StripLight::brightnessMax();
        settings.stripKelvinMix = StripLight::kelvinMix();
        settings.stripColdMin = StripLight::coldMin();
        settings.stripColdMax = StripLight::coldMax();
        settings.stripHotMin = StripLight::hotMin();
        settings.stripHotMax = StripLight::hotMax();
        settings.stripButtonClickMs = StripLight::buttonClickTime();
        settings.stripButtonLongMs = StripLight::buttonLongTime();
        settings.stripButtonRepeatMs = StripLight::buttonDuringTime();
        settings.stripBrightnessStep = StripLight::brightnessStep();
        settings.stripKelvinStep = StripLight::kelvinStep();
        sanitize();
    }

    void resetDefaults()
    {
        setDefaults();
        applyToRuntime();
        markDirty();
    }

    void markDirty()
    {
        if (!hasLoaded)
        {
            return;
        }

        dirty = true;
        dirtySinceMs = millis();
    }

    bool saveNow()
    {
        if (!storageAvailable)
        {
            dirty = false;
            return false;
        }

        refreshHeaderAndChecksum();
        dirty = false;
        return store.save();
    }
}
