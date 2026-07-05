#include "core/DeskHardware.h"

#include <Arduino.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "bus/DeskBus.h"
#include "bus/KeypadPeripheral.h"
#include "config/DeskConfig.h"
#include "config/pins_config.h"
#include "core/DeskBridgeVersion.h"
#include "core/DeskSettings.h"
#include "core/McuMonitor.h"
#include "core/StatusIndicator.h"
#include "input/PanelControls.h"
#include "light/NotificationPixels.h"
#include "light/StripLight.h"
#include "ui/OledPanel.h"
#include "usb/DeskUSB.h"
#include <Antenna.hpp>

namespace
{
    bool resetKeyWasPressed = false;
    bool resetKeyAlreadyHandled = false;
    uint32_t resetKeyPressedSinceMs = 0;
    bool lastBleConnected = false;
    uint32_t lastBleTelemetryMs = 0;
    uint32_t lastBleInfoMs = 0;

    bool equalsIgnoreCase(const char *left, const char *right)
    {
        while (*left != '\0' && *right != '\0')
        {
            if (tolower(static_cast<unsigned char>(*left)) != tolower(static_cast<unsigned char>(*right)))
            {
                return false;
            }
            ++left;
            ++right;
        }

        return *left == '\0' && *right == '\0';
    }

    bool parseU16(const char *text, uint16_t &value)
    {
        if (text == nullptr || *text == '\0')
        {
            return false;
        }

        char *end = nullptr;
        const unsigned long parsed = strtoul(text, &end, 10);
        if (end == text || *end != '\0' || parsed > StripLight::pwmMax())
        {
            return false;
        }

        value = static_cast<uint16_t>(parsed);
        return true;
    }

    bool parseBool(const char *text, bool &value)
    {
        if (equalsIgnoreCase(text, "1") || equalsIgnoreCase(text, "on") || equalsIgnoreCase(text, "true") || equalsIgnoreCase(text, "yes"))
        {
            value = true;
            return true;
        }

        if (equalsIgnoreCase(text, "0") || equalsIgnoreCase(text, "off") || equalsIgnoreCase(text, "false") || equalsIgnoreCase(text, "no"))
        {
            value = false;
            return true;
        }

        return false;
    }

    bool parseLightMode(const char *text, StripLight::Mode &mode)
    {
        if (equalsIgnoreCase(text, "0") || equalsIgnoreCase(text, "one") || equalsIgnoreCase(text, "simple"))
        {
            mode = StripLight::Mode::OneSimple;
            return true;
        }

        if (equalsIgnoreCase(text, "1") || equalsIgnoreCase(text, "two") || equalsIgnoreCase(text, "dual"))
        {
            mode = StripLight::Mode::TwoSimple;
            return true;
        }

        if (equalsIgnoreCase(text, "2") || equalsIgnoreCase(text, "composite"))
        {
            mode = StripLight::Mode::Composite;
            return true;
        }

        return false;
    }

    void beginConfigResetKey()
    {
        if (BUILTIN_KEY_PIN == NC)
        {
            return;
        }

        pinMode(BUILTIN_KEY_PIN, INPUT_PULLDOWN);
    }

    void updateConfigResetKey()
    {
        if (BUILTIN_KEY_PIN == NC)
        {
            return;
        }

        const bool pressed = digitalRead(BUILTIN_KEY_PIN) == HIGH;
        const uint32_t now = millis();

        if (pressed && !resetKeyWasPressed)
        {
            resetKeyWasPressed = true;
            resetKeyAlreadyHandled = false;
            resetKeyPressedSinceMs = now;
        }
        else if (!pressed)
        {
            resetKeyWasPressed = false;
            resetKeyAlreadyHandled = false;
        }

        if (pressed && !resetKeyAlreadyHandled && now - resetKeyPressedSinceMs >= CONFIG_RESET_KEY_HOLD_MS_DEFAULT)
        {
            DeskSettings::resetDefaults();
            DeskSettings::saveNow();
            KeypadPeripheral::syncStripConfig();
            resetKeyAlreadyHandled = true;
        }
    }

    void printFloatOrNan(char *target, size_t targetSize, float value, uint8_t decimals)
    {
        if (target == nullptr || targetSize == 0)
        {
            return;
        }

        if (isnan(value))
        {
            snprintf(target, targetSize, "nan");
            return;
        }

        char valueText[16] = {};
        dtostrf(value, 0, decimals, valueText);
        snprintf(target, targetSize, "%s", valueText);
    }

    void bleWriteInfo()
    {
        char payload[192] = {};
        snprintf(payload, sizeof(payload),
                 "[INF] device: name:%s, firmware:%s, protocol:0x%02X, product_id:%s, uptime_s:%lu",
                 DeskBridgeVersion::DEVICE_NAME,
                 DeskBridgeVersion::FIRMWARE,
                 DeskBridgeVersion::SERIAL_PROTOCOL,
                 McuMonitor::uniqueIdHex(),
                 static_cast<unsigned long>(McuMonitor::uptimeSeconds()));
        WirelessAntenna.bleWrite(payload);
    }

    void bleWriteSensors()
    {
        const DeskBus::Measurements &measurements = DeskBus::measurements();
        char temperature[16] = {};
        char humidity[16] = {};
        char lux[16] = {};
        printFloatOrNan(temperature, sizeof(temperature), measurements.temperature, 1);
        printFloatOrNan(humidity, sizeof(humidity), measurements.humidity, 1);
        printFloatOrNan(lux, sizeof(lux), measurements.lux, 1);

        char sensorsPayload[192] = {};
        snprintf(sensorsPayload, sizeof(sensorsPayload),
                 "[DAT] sensors: temp_c:%s, humidity_pct:%s, co2_ppm:%d, lux:%s, tvoc_ppb:%d, aqi:%u",
                 temperature,
                 humidity,
                 measurements.co2Ppm,
                 lux,
                 measurements.tvocPpb,
                 measurements.airQualityIndex);
        WirelessAntenna.bleWrite(sensorsPayload);
    }

    void bleWriteConfig()
    {
        const DeskSettings::Config &settings = DeskSettings::dataConst();

        char configPayload[160] = {};
        snprintf(configPayload, sizeof(configPayload),
                 "[DAT] config: sample_ms:%lu, samples:%u, temp_unit:%u, display_design:%u, display_fps:%u, usb:%s",
                 static_cast<unsigned long>(settings.sensorSampleIntervalMs),
                 settings.sensorSampleCount,
                 settings.temperatureUnit,
                 settings.displayDesign,
                 settings.displayFrameRateFps,
                 DeskUSB::mounted() ? "mounted" : "off");
        WirelessAntenna.bleWrite(configPayload);
    }

    void bleWriteLight()
    {
        char payload[192] = {};
        snprintf(payload, sizeof(payload),
                 "[DAT] light: enabled:%s, mode:%s, brightness:%u, kelvin:%u, cold_min:%u, cold_max:%u, warm_min:%u, warm_max:%u",
                 StripLight::enabled() ? "yes" : "no",
                 StripLight::modeName(),
                 StripLight::brightness(),
                 StripLight::kelvinMix(),
                 StripLight::coldMin(),
                 StripLight::coldMax(),
                 StripLight::hotMin(),
                 StripLight::hotMax());
        WirelessAntenna.bleWrite(payload);
    }

    void commitLightChange()
    {
        DeskSettings::captureStripFromRuntime();
        DeskSettings::markDirty();
        KeypadPeripheral::syncStripConfig();
        bleWriteLight();
    }

    void bleWriteError(const char *text)
    {
        char payload[96] = {};
        snprintf(payload, sizeof(payload), "[ERR] ble.%s", text ? text : "error");
        WirelessAntenna.bleWrite(payload);
    }

    void handleBleLightCommand(char *field, char *value)
    {
        if (field == nullptr || *field == '\0' || equalsIgnoreCase(field, "?") || equalsIgnoreCase(field, "state"))
        {
            bleWriteLight();
            return;
        }

        bool boolValue = false;
        uint16_t numericValue = 0;

        if (equalsIgnoreCase(field, "enabled"))
        {
            if (value == nullptr || !parseBool(value, boolValue))
            {
                bleWriteError("light.enabled.invalid");
                return;
            }
            StripLight::setEnabled(boolValue);
            commitLightChange();
            return;
        }

        if (!StripLight::enabled())
        {
            bleWriteError("light.disabled");
            return;
        }

        if (equalsIgnoreCase(field, "brightness"))
        {
            if (value == nullptr || !parseU16(value, numericValue))
            {
                bleWriteError("light.brightness.invalid");
                return;
            }
            StripLight::setBrightness(numericValue);
            commitLightChange();
            return;
        }

        if (equalsIgnoreCase(field, "kelvin"))
        {
            if (value == nullptr || !parseU16(value, numericValue))
            {
                bleWriteError("light.kelvin.invalid");
                return;
            }
            StripLight::setKelvinMix(numericValue);
            commitLightChange();
            return;
        }

        if (equalsIgnoreCase(field, "mode"))
        {
            StripLight::Mode mode = StripLight::Mode::Composite;
            if (value == nullptr || !parseLightMode(value, mode))
            {
                bleWriteError("light.mode.invalid");
                return;
            }
            StripLight::setMode(mode);
            commitLightChange();
            return;
        }

        if (equalsIgnoreCase(field, "sync"))
        {
            KeypadPeripheral::syncStripConfig();
            bleWriteLight();
            return;
        }

        bleWriteError("light.command.unknown");
    }

    void processBleCommand()
    {
        if (!WirelessAntenna.bleRxPending())
        {
            return;
        }

        char command[96] = {};
        strncpy(command, WirelessAntenna.bleLastRx(), sizeof(command) - 1);
        WirelessAntenna.clearBleRx();

        char *cursor = command;
        while (*cursor == ' ' || *cursor == '\t')
        {
            ++cursor;
        }

        char *verb = strtok(cursor, " \t\r\n");
        char *arg1 = strtok(nullptr, " \t\r\n");
        char *arg2 = strtok(nullptr, " \t\r\n");

        if (verb == nullptr)
        {
            return;
        }

        if (equalsIgnoreCase(verb, "info") || equalsIgnoreCase(verb, "info?") || equalsIgnoreCase(verb, "program") || equalsIgnoreCase(verb, "program?"))
        {
            bleWriteInfo();
            return;
        }

        if (equalsIgnoreCase(verb, "sensors") || equalsIgnoreCase(verb, "sensors?") || equalsIgnoreCase(verb, "sensor") || equalsIgnoreCase(verb, "sensor?"))
        {
            bleWriteSensors();
            return;
        }

        if (equalsIgnoreCase(verb, "config") || equalsIgnoreCase(verb, "config?"))
        {
            bleWriteConfig();
            return;
        }

        if (equalsIgnoreCase(verb, "light") || equalsIgnoreCase(verb, "light?"))
        {
            handleBleLightCommand(arg1, arg2);
            return;
        }

        bleWriteError("command.unknown");
    }

    void publishBleTelemetry()
    {
        const bool bleConnected = WirelessAntenna.bleConnected();
        const uint32_t now = millis();

        if (!bleConnected)
        {
            lastBleConnected = false;
            return;
        }

        const bool firstConnectionFrame = !lastBleConnected;
        if (firstConnectionFrame || now - lastBleInfoMs >= WIRELESS_BLE_INFO_INTERVAL_MS_DEFAULT)
        {
            lastBleInfoMs = now;
            bleWriteInfo();
        }

        if (!firstConnectionFrame && now - lastBleTelemetryMs < WIRELESS_BLE_TELEMETRY_INTERVAL_MS_DEFAULT)
        {
            return;
        }

        lastBleConnected = true;
        lastBleTelemetryMs = now;

        bleWriteSensors();
        bleWriteConfig();
        if (StripLight::enabled())
        {
            bleWriteLight();
        }
    }
}

namespace DeskHardware
{
    void begin()
    {
        beginConfigResetKey();
        StatusIndicator::begin();
        StripLight::begin();
        NotificationPixels::begin();
        PanelControls::begin();
        McuMonitor::begin();
        WirelessAntenna.begin();
#if WIRELESS_BLE_AUTOSTART_DEFAULT
        WirelessAntenna.setEnabled(true);
        WirelessAntenna.setMode(Antenna::Mode::BleDevice);
        WirelessAntenna.apply();
#endif
        OledPanel::begin();
        OledPanel::bootBegin();

        OledPanel::showBootStatus(DeskUSB::mounted() ? "USB=OK" : "USB=WAIT");
        OledPanel::showBootStatus("SPI=OK");
        OledPanel::showBootStatus("OLED=OK");
        OledPanel::showBootStatus("NeoPixel=OK");

        DeskBus::begin();
#if defined(ARDUINO_ARCH_ESP32)
        OledPanel::showBootStatus("I2C GPIO35/36");
        OledPanel::showBootStatus("KEYPAD=NC");
#else
        OledPanel::showBootStatus("I2C PB9/PB8");
        OledPanel::showBootStatus("KEY UART PA2/PA3");
#endif
        OledPanel::showBootStatus(DeskBus::ready(DeskBus::Device::DS3231) ? "DS3231=OK" : "DS3231=MISS");
        OledPanel::showBootStatus(DeskBus::ready(DeskBus::Device::AT24C32) ? "AT24C32=OK" : "AT24C32=MISS");
        OledPanel::showBootStatus(DeskBus::ready(DeskBus::Device::Keypad) ? "KEYPAD=OK" : "KEYPAD=MISS");
        OledPanel::showBootStatus(DeskBus::ready(DeskBus::Device::ENS160) ? "ENS160=OK" : "ENS160=MISS");
        OledPanel::showBootStatus(DeskBus::ready(DeskBus::Device::AHT21) ? "AHT21=OK" : "AHT21=MISS");
        OledPanel::showBootStatus(DeskBus::ready(DeskBus::Device::VEML7700) ? "VEML7700=OK" : "VEML7700=MISS");
        OledPanel::showBootStatus(DeskBus::ready(DeskBus::Device::SCD41) ? "SCD41=OK" : "SCD41=MISS");
        OledPanel::showBootStatus(DeskBus::ready(DeskBus::Device::SHT21) ? "SHT21=OK" : "SHT21=MISS");
        DeskSettings::begin();
        OledPanel::showBootStatus(DeskSettings::storageReady() ? "EEPROM CFG=OK" : "EEPROM CFG=RAM");
        OledPanel::bootFinish();
    }

    void updateBackend()
    {
        StripLight::update();
        KeypadPeripheral::serviceEvents();
        DeskBus::update();
        DeskSettings::update();
        WirelessAntenna.update();
        processBleCommand();
        publishBleTelemetry();
        McuMonitor::update();
        StatusIndicator::update();
    }

    void updateInterface()
    {
        updateConfigResetKey();
        PanelControls::update();
        OledPanel::update();
        NotificationPixels::update();
    }

    void update()
    {
        updateBackend();
        updateInterface();
    }
}
