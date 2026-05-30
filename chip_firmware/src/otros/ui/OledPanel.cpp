#include "ui/OledPanel.h"

#include <Arduino.h>

#include "bus/DeskBus.h"
#include "config/DeskConfig.h"
#include "core/DeskBridgeVersion.h"
#include "core/DeskSettings.h"
#include "core/McuMonitor.h"
#include "core/StatusIndicator.h"
#include "input/PanelControls.h"
#include "light/NotificationPixels.h"
#include "light/StripLight.h"
#include "ui/OledDisplay.h"
#include "usb/DeskUSB.h"

namespace
{
    enum class Screen : uint8_t
    {
        Home,
        MainMenu,
        UsbInfo,
        LightMenu,
        LightEnabled,
        LightMode,
        LightBrightnessMenu,
        LightBrightnessValue,
        LightBrightnessMin,
        LightBrightnessMax,
        LightKelvinMenu,
        LightKelvinValue,
        LightKelvinColdMin,
        LightKelvinColdMax,
        LightKelvinHotMin,
        LightKelvinHotMax,
        LightButtonsMenu,
        LightButtonClickTime,
        LightButtonLongTime,
        LightButtonDuringTime,
        LightBrightnessStep,
        LightKelvinStep,
        BusMenu,
        BusScan,
        BusSensorsMenu,
        BusDeviceStatus,
        RtcMenu,
        RtcSetTime,
        RtcSetDate,
        RtcAlarmsMenu,
        RtcAlarmList,
        RtcCreateAlarm,
        RtcAlarmMenu,
        RtcAlarmEdit,
        RtcAlarmNotification,
        RtcAlarmDelete,
        SystemMenu,
        SystemBusScan,
        ChipInfo,
        DisplayTimeout,
        TemperatureUnit,
        SystemSensorsMenu,
        SensorSampleInterval,
        SensorSampleCount,
        SystemNotificationsMenu,
        SystemNotificationList,
        SystemNotificationMenu,
        SystemNotificationEdit,
        SystemNotificationDelete,
        SystemHomeViewMenu,
    };

    enum class ParentMenu : uint8_t
    {
        System,
        Bus,
    };

    enum MainOption : uint8_t
    {
        MainUsbInfo,
        MainLight,
        MainBus,
        MainRtc,
        MainSystem,
        MainOptionCount,
    };

    enum LightOption : uint8_t
    {
        LightEnabledOption,
        LightModeOption,
        LightBrightnessOption,
        LightKelvinOption,
        LightButtonsOption,
        LightBack,
        LightOptionCount,
    };

    enum LightBrightnessOption : uint8_t
    {
        LightBrightnessValueOption,
        LightBrightnessMinOption,
        LightBrightnessMaxOption,
        LightBrightnessBack,
        LightBrightnessOptionCount,
    };

    enum LightKelvinOption : uint8_t
    {
        LightKelvinValueOption,
        LightKelvinColdMinOption,
        LightKelvinColdMaxOption,
        LightKelvinHotMinOption,
        LightKelvinHotMaxOption,
        LightKelvinBack,
        LightKelvinOptionCount,
    };

    enum LightButtonOption : uint8_t
    {
        LightButtonClickTimeOption,
        LightButtonLongTimeOption,
        LightButtonDuringTimeOption,
        LightButtonBrightnessStepOption,
        LightButtonKelvinStepOption,
        LightButtonBack,
        LightButtonOptionCount,
    };

    enum BusOption : uint8_t
    {
        BusScanOption,
        BusSensorsOption,
        BusSettingsOption,
        BusBack,
        BusOptionCount,
    };

    enum BusSensorOption : uint8_t
    {
        BusEns160,
        BusAht21,
        BusVeml7700,
        BusScd41,
        BusSht21,
        BusSensorBack,
        BusSensorOptionCount,
    };

    enum RtcOption : uint8_t
    {
        RtcAdjustTime,
        RtcAdjustDate,
        RtcAdjustAlarms,
        RtcBack,
        RtcOptionCount,
    };

    enum RtcAlarmOption : uint8_t
    {
        RtcAlarmListOption,
        RtcAlarmCreateOption,
        RtcAlarmBack,
        RtcAlarmOptionCount,
    };

    enum RtcAlarmEditOption : uint8_t
    {
        RtcAlarmEditOptionItem,
        RtcAlarmNotificationOption,
        RtcAlarmDeleteOption,
        RtcAlarmEditBack,
        RtcAlarmEditOptionCount,
    };

    enum SystemOption : uint8_t
    {
        SystemDisplayTimeout,
        SystemScanBus,
        SystemChipInfo,
        SystemNotifications,
        SystemHomeView,
        SystemBack,
        SystemOptionCount,
    };

    enum SystemNotificationOption : uint8_t
    {
        SystemNotificationBusAlarm,
        SystemNotificationTempRange,
        SystemNotificationHumidityRange,
        SystemNotificationCo2Range,
        SystemNotificationRuntime,
        SystemNotificationBack,
        SystemNotificationOptionCount,
    };

    enum SystemNotificationEditOption : uint8_t
    {
        SystemNotificationEditOptionItem,
        SystemNotificationDeleteOption,
        SystemNotificationEditBack,
        SystemNotificationEditOptionCount,
    };

    enum SystemSensorOption : uint8_t
    {
        SystemSensorSampleInterval,
        SystemSensorSampleCount,
        SystemSensorUnits,
        SystemSensorBack,
        SystemSensorOptionCount,
    };

    enum HomeViewOption : uint8_t
    {
        HomeViewBoxBig,
        HomeViewBoxSmall,
        HomeViewBoxData1,
        HomeViewBoxData2,
        HomeViewBoxData3,
        HomeViewBack,
        HomeViewOptionCount,
    };

    enum class HomeField : uint8_t
    {
        Time,
        Date,
        Temperature,
        Humidity,
        Co2,
        Lux,
        Tvoc,
        Eco2,
        Aqi,
        ShtTemperature,
        ShtHumidity,
        Usb,
        Empty,
        Count,
    };

    const char *const mainOptions[MainOptionCount] = {
        "USB info",
        "Light",
        "Bus",
        "RTC",
        "System",
    };

    const char *const lightOptions[LightOptionCount] = {
        "Enabled",
        "Mode",
        "Brightness",
        "Kelvin",
        "Buttons",
        "Back",
    };

    const char *const lightBrightnessOptions[LightBrightnessOptionCount] = {
        "Value",
        "Min",
        "Max",
        "Back",
    };

    const char *const lightKelvinOptions[LightKelvinOptionCount] = {
        "Value",
        "ColdMin",
        "ColdMax",
        "HotMin",
        "HotMax",
        "Back",
    };

    const char *const lightButtonOptions[LightButtonOptionCount] = {
        "Click Time",
        "Long Time",
        "During Time",
        "Brightness step",
        "Kelvin step",
        "Back",
    };

    const char *const busOptions[BusOptionCount] = {
        "Scan",
        "Sensors",
        "Settings",
        "Back",
    };

    const char *const busSensorOptions[BusSensorOptionCount] = {
        "ENS160",
        "AHT21",
        "VEML7700",
        "SCD41",
        "SHT21",
        "Back",
    };

    const char *const rtcOptions[RtcOptionCount] = {
        "Ajustar Hora",
        "Ajustar Fecha",
        "Ajustar Alarmas",
        "Back",
    };

    const char *const rtcAlarmOptions[RtcAlarmOptionCount] = {
        "Alarmas",
        "Crear Alarma",
        "Back",
    };

    const char *const rtcAlarmEditOptions[RtcAlarmEditOptionCount] = {
        "Editar",
        "Vincular Notify",
        "Eliminar",
        "Back",
    };

    const char *const systemOptions[SystemOptionCount] = {
        "Display timeout",
        "Scan system bus",
        "Chip info",
        "Notifications",
        "Home View",
        "Back",
    };

    const char *const systemSensorOptions[SystemSensorOptionCount] = {
        "Sample interval",
        "Samples count",
        "Units",
        "Back",
    };

    const char *const temperatureUnitNames[] = {
        "Celsius",
        "Fahrenheit",
    };

    const char *const notificationOptions[SystemNotificationOptionCount] = {
        "Bus element alarm",
        "Temp range alarm",
        "Humidity range",
        "CO2 range alarm",
        "Runtime alarm",
        "Back",
    };

    const char *const notificationEditOptions[SystemNotificationEditOptionCount] = {
        "Editar",
        "Eliminar",
        "Back",
    };

    const char *const homeViewOptions[HomeViewOptionCount] = {
        "Box Big",
        "Box Small",
        "Box Data 1",
        "Box Data 2",
        "Box Data 3",
        "Back",
    };

    const char *const homeFieldNames[static_cast<uint8_t>(HomeField::Count)] = {
        "Hora",
        "Fecha",
        "Temperatura",
        "Humedad",
        "CO2",
        "Lux",
        "TVOC",
        "eCO2",
        "AQI",
        "SHT Temp",
        "SHT Hum",
        "USB",
        "Vacio",
    };

    Screen activeScreen = Screen::Home;
    bool needsRedraw = true;
    uint32_t lastDrawMs = 0;
    uint32_t lastInteractionMs = 0;
    const char *bootLines[3] = {};
    uint8_t bootLineCount = 0;
    uint32_t bootStartMs = 0;
    constexpr uint32_t bootDurationMs = DISPLAY_BOOT_DURATION_MS_DEFAULT;
    constexpr uint32_t inputSensorPauseMs = DISPLAY_INPUT_SENSOR_PAUSE_MS_DEFAULT;
    constexpr uint32_t homeRedrawIntervalMs = DISPLAY_HOME_REDRAW_INTERVAL_MS_DEFAULT;
    constexpr uint32_t statusRedrawIntervalMs = DISPLAY_STATUS_REDRAW_INTERVAL_MS_DEFAULT;

    uint8_t selectedMain = MainUsbInfo;
    uint8_t selectedLight = LightEnabledOption;
    uint8_t selectedLightBrightness = LightBrightnessValueOption;
    uint8_t selectedLightKelvin = LightKelvinValueOption;
    uint8_t selectedLightButton = LightButtonClickTimeOption;
    uint8_t selectedBus = BusScanOption;
    uint8_t selectedBusSensor = BusEns160;
    uint8_t selectedRtc = RtcAdjustTime;
    uint8_t selectedRtcAlarm = RtcAlarmListOption;
    uint8_t selectedRtcAlarmEdit = RtcAlarmEditOptionItem;
    uint8_t selectedSystem = SystemDisplayTimeout;
    uint8_t selectedSystemSensor = SystemSensorSampleInterval;
    uint8_t selectedNotification = SystemNotificationBusAlarm;
    uint8_t selectedNotificationEdit = SystemNotificationEditOptionItem;
    uint8_t selectedHomeView = HomeViewBoxBig;
    ParentMenu sensorConfigParent = ParentMenu::Bus;

    uint8_t scannedAddresses[12] = {};
    uint8_t scannedAddressCount = 0;

    uint16_t rtcEditYear = 2026;
    uint8_t rtcEditMonth = 1;
    uint8_t rtcEditDay = 1;
    uint8_t rtcEditHour = 0;
    uint8_t rtcEditMinute = 0;
    uint8_t rtcEditSecond = 0;
    uint8_t rtcEditField = 0;
    bool rtcEditLoaded = false;
    bool rtcEditSaved = false;

    void markInteraction()
    {
        lastInteractionMs = millis();
        DeskBus::pauseSampling(inputSensorPauseMs);
        needsRedraw = true;
    }

    uint32_t periodicRedrawInterval()
    {
        switch (activeScreen)
        {
            case Screen::Home:
                return homeRedrawIntervalMs;
            case Screen::UsbInfo:
            case Screen::ChipInfo:
            case Screen::SystemBusScan:
            case Screen::BusMenu:
            case Screen::BusSensorsMenu:
            case Screen::BusDeviceStatus:
            case Screen::RtcMenu:
            case Screen::SensorSampleInterval:
            case Screen::SensorSampleCount:
                return statusRedrawIntervalMs;
            default:
                return 0;
        }
    }

    void markStripConfigChanged()
    {
        DeskSettings::captureStripFromRuntime();
        DeskSettings::markDirty();
    }

    void drawCentered(const char *text, int y)
    {
        const int width = Oled.getStrWidth(text);
        Oled.drawStr((128 - width) / 2, y, text);
    }

    bool anyBusDeviceMissing()
    {
        return !DeskBus::ready(DeskBus::Device::ENS160) ||
               !DeskBus::ready(DeskBus::Device::AHT21) ||
               !DeskBus::ready(DeskBus::Device::VEML7700) ||
               !DeskBus::ready(DeskBus::Device::SCD41) ||
               !DeskBus::ready(DeskBus::Device::SHT21);
    }

    bool anyBusDeviceFault()
    {
        if (millis() < 7000)
        {
            return false;
        }

        const DeskBus::Measurements &data = DeskBus::measurements();
        return (DeskBus::ready(DeskBus::Device::AHT21) && (isnan(data.temperature) || isnan(data.humidity))) ||
               (DeskBus::ready(DeskBus::Device::VEML7700) && isnan(data.lux)) ||
               (DeskBus::ready(DeskBus::Device::SCD41) && data.co2Ppm < 0) ||
               (DeskBus::ready(DeskBus::Device::SHT21) && (isnan(data.shtTemperature) || isnan(data.shtHumidity)));
    }

    void drawTaskbarIcons()
    {
        Oled.setFont(OledFont::Small);

        int x = 126;
        if (DeskUSB::vbusPresent())
        {
            x -= 18;
            Oled.drawStr(x, 7, "USB");
        }

        if (anyBusDeviceFault())
        {
            x -= 7;
            Oled.drawStr(x, 7, "!");
        }
        else if (anyBusDeviceMissing())
        {
            x -= 7;
            Oled.drawStr(x, 7, "?");
        }
    }

    void drawTitle(const char *title)
    {
        Oled.setFont(OledFont::Small);
        Oled.drawStr(0, 7, title);
        drawTaskbarIcons();
        Oled.drawHLine(0, 9, 128);
    }

    void replaceDotWithComma(char *text)
    {
        for (char *current = text; *current != '\0'; ++current)
        {
            if (*current == '.')
            {
                *current = ',';
            }
        }
    }

    const char *temperatureUnitName()
    {
        return temperatureUnitNames[DeskSettings::data().temperatureUnit > 1 ? 0 : DeskSettings::data().temperatureUnit];
    }

    float displayedTemperature(float celsius)
    {
        return DeskSettings::data().temperatureUnit == 1 ? (celsius * 9.0f / 5.0f) + 32.0f : celsius;
    }

    char temperatureUnitLetter()
    {
        return DeskSettings::data().temperatureUnit == 1 ? 'F' : 'C';
    }

    void formatTemperatureValue(float celsius, char *buffer, size_t length)
    {
        if (isnan(celsius))
        {
            snprintf(buffer, length, "--,-%c%c", 176, temperatureUnitLetter());
            return;
        }

        char value[8];
        dtostrf(displayedTemperature(celsius), 4, 1, value);
        replaceDotWithComma(value);
        snprintf(buffer, length, "%s%c%c", value, 176, temperatureUnitLetter());
    }

    float primaryTemperature()
    {
        const DeskBus::Measurements &data = DeskBus::measurements();
        return !isnan(data.temperature) ? data.temperature : data.shtTemperature;
    }

    float primaryHumidity()
    {
        const DeskBus::Measurements &data = DeskBus::measurements();
        return !isnan(data.humidity) ? data.humidity : data.shtHumidity;
    }

    void drawBoot()
    {
        const int barW = 56;
        const int barH = 8;
        const int barX = (128 - barW) / 2;
        const int barY = 50;
        const uint32_t elapsed = min(millis() - bootStartMs, bootDurationMs);
        const int fillW = map(elapsed, 0, bootDurationMs, 0, barW - 2);
        const uint8_t bootPixelSteps = 9;
        const uint8_t bootPixelStep = min(static_cast<uint32_t>(bootPixelSteps - 1),
                                          elapsed * bootPixelSteps / bootDurationMs);

        NotificationPixels::showBootStep(bootPixelStep);

        Oled.clearBuffer();
        Oled.setFont(OledFont::Small);

        for (uint8_t i = 0; i < bootLineCount; ++i)
        {
            Oled.drawStr(0, 7 + i * 8, bootLines[i]);
        }

        Oled.setFont(OledFont::BootTitle);
        drawCentered("DeskBridge", 43);
        Oled.drawFrame(barX, barY, barW, barH);
        Oled.drawBox(barX + 1, barY + 1, fillW, barH - 2);
        Oled.sendBuffer();
    }

    void animateBootFor(uint32_t timeMs)
    {
        const uint32_t startMs = millis();

        while (millis() - startMs < timeMs && millis() - bootStartMs < bootDurationMs)
        {
            DeskUSB::task();
            drawBoot();
            delay(DISPLAY_BOOT_FRAME_MS_DEFAULT);
        }
    }

    void moveSelection(uint8_t &selection, uint8_t count, int16_t delta)
    {
        while (delta > 0 && selection + 1 < count)
        {
            ++selection;
            --delta;
        }
        while (delta < 0 && selection > 0)
        {
            --selection;
            ++delta;
        }
    }

    int32_t adjustedValue(int32_t value, int16_t delta, int32_t step, int32_t minimum, int32_t maximum)
    {
        value += static_cast<int32_t>(delta) * step;
        return constrain(value, minimum, maximum);
    }

    bool leapYear(uint16_t year)
    {
        return ((year % 4 == 0) && (year % 100 != 0)) || (year % 400 == 0);
    }

    uint8_t daysInMonth(uint16_t year, uint8_t month)
    {
        static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        if (month == 2 && leapYear(year))
        {
            return 29;
        }

        return days[constrain(month, 1, 12) - 1];
    }

    void clampRtcDate()
    {
        rtcEditMonth = static_cast<uint8_t>(constrain(rtcEditMonth, 1, 12));
        rtcEditDay = static_cast<uint8_t>(constrain(rtcEditDay, 1, daysInMonth(rtcEditYear, rtcEditMonth)));
    }

    void loadRtcEdit()
    {
        const DateTime value = DeskBus::now();
        rtcEditYear = value.year();
        rtcEditMonth = value.month();
        rtcEditDay = value.day();
        rtcEditHour = value.hour();
        rtcEditMinute = value.minute();
        rtcEditSecond = value.second();
        rtcEditField = 0;
        rtcEditLoaded = true;
        rtcEditSaved = false;
        clampRtcDate();
    }

    void openRtcEdit(Screen screen)
    {
        loadRtcEdit();
        activeScreen = screen;
    }

    bool saveRtcEdit()
    {
        clampRtcDate();
        rtcEditSaved = DeskBus::setRtc(DateTime(rtcEditYear, rtcEditMonth, rtcEditDay, rtcEditHour, rtcEditMinute, rtcEditSecond));
        rtcEditLoaded = false;
        return rtcEditSaved;
    }

    void moveRtcEdit(int16_t delta)
    {
        if (!rtcEditLoaded)
        {
            loadRtcEdit();
        }

        if (activeScreen == Screen::RtcSetTime)
        {
            switch (rtcEditField)
            {
                case 0:
                    rtcEditHour = static_cast<uint8_t>(adjustedValue(rtcEditHour, delta, 1, 0, 23));
                    break;
                case 1:
                    rtcEditMinute = static_cast<uint8_t>(adjustedValue(rtcEditMinute, delta, 1, 0, 59));
                    break;
                default:
                    rtcEditSecond = static_cast<uint8_t>(adjustedValue(rtcEditSecond, delta, 1, 0, 59));
                    break;
            }
        }
        else
        {
            switch (rtcEditField)
            {
                case 0:
                    rtcEditDay = static_cast<uint8_t>(adjustedValue(rtcEditDay, delta, 1, 1, daysInMonth(rtcEditYear, rtcEditMonth)));
                    break;
                case 1:
                    rtcEditMonth = static_cast<uint8_t>(adjustedValue(rtcEditMonth, delta, 1, 1, 12));
                    clampRtcDate();
                    break;
                default:
                    rtcEditYear = static_cast<uint16_t>(adjustedValue(rtcEditYear, delta, 1, 2024, 2099));
                    clampRtcDate();
                    break;
            }
        }
    }

    void nextRtcEditField()
    {
        constexpr uint8_t maxField = 2;
        rtcEditField = rtcEditField >= maxField ? 0 : rtcEditField + 1;
    }

    const char *homeFieldName(HomeField field)
    {
        return homeFieldNames[static_cast<uint8_t>(field)];
    }

    HomeField storedHomeField(uint8_t value)
    {
        if (value >= static_cast<uint8_t>(HomeField::Count))
        {
            return HomeField::Empty;
        }

        return static_cast<HomeField>(value);
    }

    HomeField moveHomeFieldValue(HomeField field, int16_t delta)
    {
        int16_t value = static_cast<int16_t>(field);
        value += delta > 0 ? 1 : -1;

        if (value < 0)
        {
            value = static_cast<int16_t>(HomeField::Count) - 1;
        }
        else if (value >= static_cast<int16_t>(HomeField::Count))
        {
            value = 0;
        }

        return static_cast<HomeField>(value);
    }

    uint8_t &selectedHomeFieldStorage()
    {
        DeskSettings::Config &settings = DeskSettings::data();
        switch (selectedHomeView)
        {
            case HomeViewBoxBig:
                return settings.homeBoxBig;
            case HomeViewBoxSmall:
                return settings.homeBoxSmall;
            case HomeViewBoxData1:
                return settings.homeBoxData1;
            case HomeViewBoxData2:
                return settings.homeBoxData2;
            case HomeViewBoxData3:
                return settings.homeBoxData3;
            default:
                return settings.homeBoxBig;
        }
    }

    void moveSelectedHomeField(int16_t delta)
    {
        uint8_t &storage = selectedHomeFieldStorage();
        const HomeField moved = moveHomeFieldValue(storedHomeField(storage), delta);
        storage = static_cast<uint8_t>(moved);
        DeskSettings::markDirty();
    }

    void formatHomeField(HomeField field, char *buffer, size_t length)
    {
        const DeskBus::Measurements &data = DeskBus::measurements();

        switch (field)
        {
            case HomeField::Time:
                if (DeskBus::ready(DeskBus::Device::DS3231))
                {
                    const DateTime now = DeskBus::now();
                    snprintf(buffer, length, "%02u:%02u", now.hour(), now.minute());
                }
                else
                {
                    snprintf(buffer, length, "--:--");
                }
                break;
            case HomeField::Date:
                if (DeskBus::ready(DeskBus::Device::DS3231))
                {
                    const DateTime now = DeskBus::now();
                    snprintf(buffer, length, "%02u/%02u/%04u", now.day(), now.month(), now.year());
                }
                else
                {
                    snprintf(buffer, length, "--/--/----");
                }
                break;
            case HomeField::Temperature:
                formatTemperatureValue(primaryTemperature(), buffer, length);
                break;
            case HomeField::Humidity:
            {
                const float humidity = primaryHumidity();
                if (isnan(humidity))
                {
                    snprintf(buffer, length, "--.- %%");
                }
                else
                {
                    char value[8];
                    dtostrf(humidity, 4, 1, value);
                    snprintf(buffer, length, "%s %%", value);
                }
                break;
            }
            case HomeField::Co2:
                if (data.co2Ppm < 0)
                {
                    snprintf(buffer, length, "---- ppm");
                }
                else
                {
                    snprintf(buffer, length, "%04d ppm", data.co2Ppm);
                }
                break;
            case HomeField::Lux:
                if (isnan(data.lux))
                {
                    snprintf(buffer, length, "---- lx");
                }
                else
                {
                    snprintf(buffer, length, "%04u lx", static_cast<unsigned int>(data.lux));
                }
                break;
            case HomeField::Tvoc:
                snprintf(buffer, length, data.tvocPpb < 0 ? "---- ppb" : "%04d ppb", data.tvocPpb);
                break;
            case HomeField::Eco2:
                snprintf(buffer, length, data.ensEco2Ppm < 0 ? "---- ppm" : "%04d ppm", data.ensEco2Ppm);
                break;
            case HomeField::Aqi:
                snprintf(buffer, length, "AQI %u", data.airQualityIndex);
                break;
            case HomeField::ShtTemperature:
                formatTemperatureValue(data.shtTemperature, buffer, length);
                break;
            case HomeField::ShtHumidity:
                if (isnan(data.shtHumidity))
                {
                    snprintf(buffer, length, "--.- %%");
                }
                else
                {
                    char value[8];
                    dtostrf(data.shtHumidity, 4, 1, value);
                    snprintf(buffer, length, "%s %%", value);
                }
                break;
            case HomeField::Usb:
                snprintf(buffer, length, DeskUSB::mounted() ? "USB OK" : "USB --");
                break;
            case HomeField::Empty:
            default:
                snprintf(buffer, length, "");
                break;
        }
    }

    void drawOptionList(const char *title, const char *const *options, uint8_t count, uint8_t selected)
    {
        Oled.clearBuffer();
        drawTitle(title);

        uint8_t first = 0;
        if (selected >= DISPLAY_MENU_VISIBLE_ROWS_DEFAULT)
        {
            first = selected - (DISPLAY_MENU_VISIBLE_ROWS_DEFAULT - 1);
        }

        for (uint8_t row = 0; row < DISPLAY_MENU_VISIBLE_ROWS_DEFAULT && first + row < count; ++row)
        {
            const uint8_t option = first + row;
            const int y = 20 + row * 9;

            if (option == selected)
            {
                Oled.drawBox(0, y - 7, 128, 9);
                Oled.setDrawColor(0);
            }

            Oled.drawStr(4, y, options[option]);

            if (option == selected)
            {
                Oled.setDrawColor(1);
            }
        }

        Oled.sendBuffer();
    }

    void drawHome()
    {
        char boxBigText[18];
        char boxSmallText[18];
        char boxData1Text[18];
        char boxData2Text[18];
        char boxData3Text[18];

        formatHomeField(storedHomeField(DeskSettings::data().homeBoxBig), boxBigText, sizeof(boxBigText));
        formatHomeField(storedHomeField(DeskSettings::data().homeBoxSmall), boxSmallText, sizeof(boxSmallText));
        formatHomeField(storedHomeField(DeskSettings::data().homeBoxData1), boxData1Text, sizeof(boxData1Text));
        formatHomeField(storedHomeField(DeskSettings::data().homeBoxData2), boxData2Text, sizeof(boxData2Text));
        formatHomeField(storedHomeField(DeskSettings::data().homeBoxData3), boxData3Text, sizeof(boxData3Text));

        Oled.clearBuffer();
        Oled.drawHLine(0, 8, 128);
        Oled.setFont(OledFont::Text);
        Oled.drawStr(52, 7, "Home");
        drawTaskbarIcons();

        Oled.setFont(OledFont::HomeTime);
        Oled.drawStr(4, 36, boxBigText);

        Oled.setFont(OledFont::Date);
        Oled.drawStr(5, 58, boxSmallText);

        Oled.setFont(OledFont::Text);
        Oled.setCursor(78, 21);
        Oled.print(boxData1Text);

        Oled.setCursor(78, 38);
        Oled.print(boxData2Text);

        Oled.setFont(OledFont::Tiny);
        Oled.setCursor(78, 56);
        Oled.print(boxData3Text);
        Oled.sendBuffer();
    }

    void drawUsbInfo()
    {
        Oled.clearBuffer();
        drawTitle("USB info");
        Oled.setCursor(0, 22);
        Oled.print("Mounted: ");
        Oled.print(DeskUSB::mounted() ? "yes" : "no");
        Oled.setCursor(0, 34);
        Oled.print("VBUS: ");
        Oled.print(DeskUSB::vbusPresent() ? "yes" : "no");
        Oled.setCursor(0, 46);
        Oled.print("COM open: ");
        Oled.print(DeskUSB::cdcConnected(DeskUSB::CONTROL) ? "yes" : "no");
        Oled.setCursor(0, 58);
        Oled.print("VID 1209 PID DB01");
        Oled.sendBuffer();
    }

    void drawChipInfo()
    {
        char temperatureText[12];
        char vddaText[12];
        char clockText[14];
        char uptimeText[16];

        if (!McuMonitor::ready() || isnan(McuMonitor::temperatureC()))
        {
            snprintf(temperatureText, sizeof(temperatureText), "--,-%cC", 176);
        }
        else
        {
            char value[8];
            dtostrf(McuMonitor::temperatureC(), 4, 1, value);
            replaceDotWithComma(value);
            snprintf(temperatureText, sizeof(temperatureText), "%s%cC", value, 176);
        }

        if (!McuMonitor::ready() || isnan(McuMonitor::vddaVolts()))
        {
            snprintf(vddaText, sizeof(vddaText), "-.---V");
        }
        else
        {
            char value[8];
            dtostrf(McuMonitor::vddaVolts(), 4, 3, value);
            snprintf(vddaText, sizeof(vddaText), "%sV", value);
        }

        snprintf(clockText, sizeof(clockText), "%lu MHz", McuMonitor::coreClockHz() / 1000000UL);

        const uint32_t uptime = McuMonitor::uptimeSeconds();
        snprintf(uptimeText, sizeof(uptimeText), "%02lu:%02lu:%02lu",
                 uptime / 3600UL,
                 (uptime / 60UL) % 60UL,
                 uptime % 60UL);

        Oled.clearBuffer();
        drawTitle("Chip info");
        Oled.setFont(OledFont::Small);
        Oled.setCursor(0, 20);
        Oled.print("MCU: ");
        Oled.print(McuMonitor::chipName());
        Oled.setCursor(0, 30);
        Oled.print("Temp: ");
        Oled.print(temperatureText);
        Oled.setCursor(0, 40);
        Oled.print("VDDA: ");
        Oled.print(vddaText);
        Oled.setCursor(0, 50);
        Oled.print("Clock: ");
        Oled.print(clockText);
        Oled.setCursor(0, 60);
        Oled.print("Up: ");
        Oled.print(uptimeText);
        Oled.setCursor(78, 60);
        Oled.print(McuMonitor::resetReason());
        Oled.sendBuffer();
    }

    void drawPlaceholder(const char *title, const char *line1, const char *line2 = "Pending implementation")
    {
        Oled.clearBuffer();
        drawTitle(title);
        Oled.setCursor(0, 24);
        Oled.print(line1);
        Oled.setCursor(0, 40);
        Oled.print(line2);
        Oled.setCursor(0, 62);
        Oled.print("ESC: back");
        Oled.sendBuffer();
    }

    void drawRawEdit(const char *title, uint16_t value, const char *hint)
    {
        Oled.clearBuffer();
        drawTitle(title);
        Oled.setFont(OledFont::Value);
        Oled.setCursor(8, 38);
        Oled.print(value);
        Oled.setFont(OledFont::Small);
        Oled.drawStr(0, 54, hint);
        Oled.drawStr(0, 63, "ESC: back");
        Oled.sendBuffer();
    }

    void drawSecondsEdit(const char *title, uint32_t milliseconds, const char *hint)
    {
        char secondsText[8];
        const uint32_t tenths = (milliseconds + 50) / 100;
        snprintf(secondsText, sizeof(secondsText), "%02lu.%lu", tenths / 10, tenths % 10);

        Oled.clearBuffer();
        drawTitle(title);
        Oled.setFont(OledFont::Value);
        Oled.setCursor(8, 38);
        Oled.print(secondsText);
        Oled.print("s");
        Oled.setFont(OledFont::Small);
        Oled.drawStr(0, 54, hint);
        Oled.drawStr(0, 63, "ESC: back");
        Oled.sendBuffer();
    }

    void drawEnabledEdit()
    {
        Oled.clearBuffer();
        drawTitle("Light enabled");
        Oled.setFont(OledFont::Value);
        Oled.setCursor(8, 38);
        Oled.print(StripLight::enabled() ? "ON" : "OFF");
        Oled.setFont(OledFont::Small);
        Oled.drawStr(0, 62, "click: toggle  ESC: back");
        Oled.sendBuffer();
    }

    void drawModeEdit()
    {
        Oled.clearBuffer();
        drawTitle("Light mode");
        Oled.setFont(OledFont::Value);
        Oled.setCursor(8, 38);
        Oled.print(StripLight::modeName());
        Oled.setFont(OledFont::Small);
        Oled.drawStr(0, 62, "click: toggle  ESC: back");
        Oled.sendBuffer();
    }

    void drawTemperatureUnitEdit()
    {
        Oled.clearBuffer();
        drawTitle("Temp unit");
        Oled.setFont(OledFont::Value);
        Oled.setCursor(8, 38);
        Oled.print(temperatureUnitName());
        Oled.setFont(OledFont::Small);
        Oled.drawStr(0, 62, "click: toggle  ESC: back");
        Oled.sendBuffer();
    }

    bool busDevicePresent(uint8_t option)
    {
        switch (option)
        {
            case BusEns160:
                return DeskBus::ready(DeskBus::Device::ENS160);
            case BusAht21:
                return DeskBus::ready(DeskBus::Device::AHT21);
            case BusVeml7700:
                return DeskBus::ready(DeskBus::Device::VEML7700);
            case BusScd41:
                return DeskBus::ready(DeskBus::Device::SCD41);
            case BusSht21:
                return DeskBus::ready(DeskBus::Device::SHT21);
            default:
                return false;
        }
    }

    const char *busDeviceAddress(uint8_t option)
    {
        switch (option)
        {
            case BusEns160:
                return "0x52/0x53";
            case BusAht21:
                return "0x38";
            case BusVeml7700:
                return "0x10";
            case BusScd41:
                return "0x62";
            case BusSht21:
                return "0x40";
            default:
                return "--";
        }
    }

    void drawBusMenu()
    {
        Oled.clearBuffer();
        drawTitle("Sensor bus");

        uint8_t first = 0;
        if (selectedBus >= DISPLAY_MENU_VISIBLE_ROWS_DEFAULT)
        {
            first = selectedBus - (DISPLAY_MENU_VISIBLE_ROWS_DEFAULT - 1);
        }

        for (uint8_t row = 0; row < DISPLAY_MENU_VISIBLE_ROWS_DEFAULT && first + row < BusOptionCount; ++row)
        {
            const uint8_t option = first + row;
            const int y = 20 + row * 9;

            if (option == selectedBus)
            {
                Oled.drawBox(0, y - 7, 128, 9);
                Oled.setDrawColor(0);
            }

            Oled.drawStr(4, y, busOptions[option]);

            if (option == selectedBus)
            {
                Oled.setDrawColor(1);
            }
        }

        Oled.sendBuffer();
    }

    void drawBusSensorsMenu()
    {
        Oled.clearBuffer();
        drawTitle("Sensors");

        uint8_t first = 0;
        if (selectedBusSensor >= DISPLAY_MENU_VISIBLE_ROWS_DEFAULT)
        {
            first = selectedBusSensor - (DISPLAY_MENU_VISIBLE_ROWS_DEFAULT - 1);
        }

        for (uint8_t row = 0; row < DISPLAY_MENU_VISIBLE_ROWS_DEFAULT && first + row < BusSensorOptionCount; ++row)
        {
            const uint8_t option = first + row;
            const int y = 20 + row * 9;

            if (option == selectedBusSensor)
            {
                Oled.drawBox(0, y - 7, 128, 9);
                Oled.setDrawColor(0);
            }

            Oled.drawStr(4, y, busSensorOptions[option]);
            if (option != BusSensorBack)
            {
                Oled.drawStr(104, y, busDevicePresent(option) ? "OK" : "--");
            }

            if (option == selectedBusSensor)
            {
                Oled.setDrawColor(1);
            }
        }

        Oled.sendBuffer();
    }

    void drawBusDeviceStatus(uint8_t option)
    {
        Oled.clearBuffer();
        drawTitle(busSensorOptions[option]);
        Oled.setCursor(0, 24);
        Oled.print("Address: ");
        Oled.print(busDeviceAddress(option));
        Oled.setCursor(0, 40);
        Oled.print("State: ");
        Oled.print(busDevicePresent(option) ? "Detected" : "Missing");
        Oled.setCursor(0, 62);
        Oled.print("ESC: back");
        Oled.sendBuffer();
    }

    HomeField homeFieldForOption(uint8_t option)
    {
        const DeskSettings::Config &settings = DeskSettings::dataConst();
        switch (option)
        {
            case HomeViewBoxBig:
                return storedHomeField(settings.homeBoxBig);
            case HomeViewBoxSmall:
                return storedHomeField(settings.homeBoxSmall);
            case HomeViewBoxData1:
                return storedHomeField(settings.homeBoxData1);
            case HomeViewBoxData2:
                return storedHomeField(settings.homeBoxData2);
            case HomeViewBoxData3:
                return storedHomeField(settings.homeBoxData3);
            default:
                return HomeField::Empty;
        }
    }

    void drawHomeViewMenu()
    {
        Oled.clearBuffer();
        drawTitle("Home View");

        uint8_t first = 0;
        if (selectedHomeView >= DISPLAY_MENU_VISIBLE_ROWS_DEFAULT)
        {
            first = selectedHomeView - (DISPLAY_MENU_VISIBLE_ROWS_DEFAULT - 1);
        }

        for (uint8_t row = 0; row < DISPLAY_MENU_VISIBLE_ROWS_DEFAULT && first + row < HomeViewOptionCount; ++row)
        {
            const uint8_t option = first + row;
            const int y = 20 + row * 9;

            if (option == selectedHomeView)
            {
                Oled.drawBox(0, y - 7, 128, 9);
                Oled.setDrawColor(0);
            }

            Oled.drawStr(4, y, homeViewOptions[option]);
            if (option < HomeViewBack)
            {
                Oled.drawStr(70, y, homeFieldName(homeFieldForOption(option)));
            }

            if (option == selectedHomeView)
            {
                Oled.setDrawColor(1);
            }
        }

        Oled.sendBuffer();
    }

    void drawI2cScan(const char *title)
    {
        Oled.clearBuffer();
        drawTitle(title);
        Oled.setCursor(0, 22);
        Oled.print("Found: ");
        Oled.print(scannedAddressCount);

        Oled.setCursor(0, 36);
        for (uint8_t i = 0; i < scannedAddressCount && i < 6; ++i)
        {
            Oled.print("0x");
            if (scannedAddresses[i] < 0x10)
            {
                Oled.print("0");
            }
            Oled.print(scannedAddresses[i], HEX);
            Oled.print(" ");
        }

        Oled.setCursor(0, 62);
        Oled.print("click: rescan");
        Oled.sendBuffer();
    }

    void drawSensorStatus()
    {
        Oled.clearBuffer();
        drawTitle("Sensors");
        Oled.setCursor(0, 18);
        Oled.print("DS3231 ");
        Oled.print(DeskBus::ready(DeskBus::Device::DS3231) ? "OK" : "--");
        Oled.setCursor(64, 18);
        Oled.print("AT24C32 ");
        Oled.print(DeskBus::ready(DeskBus::Device::AT24C32) ? "OK" : "--");
        Oled.setCursor(0, 30);
        Oled.print("ENS160 ");
        Oled.print(DeskBus::ready(DeskBus::Device::ENS160) ? "OK" : "--");
        Oled.setCursor(64, 30);
        Oled.print("AHT21 ");
        Oled.print(DeskBus::ready(DeskBus::Device::AHT21) ? "OK" : "--");
        Oled.setCursor(0, 42);
        Oled.print("VEML ");
        Oled.print(DeskBus::ready(DeskBus::Device::VEML7700) ? "OK" : "--");
        Oled.setCursor(64, 42);
        Oled.print("SCD41 ");
        Oled.print(DeskBus::ready(DeskBus::Device::SCD41) ? "OK" : "--");
        Oled.setCursor(0, 54);
        Oled.print("SHT21 ");
        Oled.print(DeskBus::ready(DeskBus::Device::SHT21) ? "OK" : "--");
        Oled.setCursor(0, 62);
        Oled.print("ESC: back");
        Oled.sendBuffer();
    }

    void drawRtcStatus()
    {
        Oled.clearBuffer();
        drawTitle("RTC");
        Oled.setCursor(0, 24);
        Oled.print("DS3231 address 0x68");
        Oled.setCursor(0, 38);
        Oled.print(DeskBus::ready(DeskBus::Device::DS3231) ? "Detected" : "Missing");
        Oled.setCursor(0, 62);
        Oled.print("ESC: back");
        Oled.sendBuffer();
    }

    void drawRtcField(uint8_t field, int16_t x, int16_t y, const char *label, const char *value)
    {
        if (rtcEditField == field)
        {
            Oled.drawBox(x - 2, y - 9, 42, 11);
            Oled.setDrawColor(0);
        }

        Oled.setFont(OledFont::Small);
        Oled.drawStr(x, y - 12, label);
        Oled.setFont(OledFont::Text);
        Oled.drawStr(x, y, value);
        Oled.setDrawColor(1);
    }

    void drawRtcEditTime()
    {
        if (!rtcEditLoaded)
        {
            loadRtcEdit();
        }

        char hour[4];
        char minute[4];
        char second[4];
        snprintf(hour, sizeof(hour), "%02u", rtcEditHour);
        snprintf(minute, sizeof(minute), "%02u", rtcEditMinute);
        snprintf(second, sizeof(second), "%02u", rtcEditSecond);

        Oled.clearBuffer();
        drawTitle("Ajustar Hora");
        drawRtcField(0, 5, 34, "HH", hour);
        drawRtcField(1, 46, 34, "MM", minute);
        drawRtcField(2, 87, 34, "SS", second);
        Oled.setFont(OledFont::Small);
        Oled.drawStr(0, 54, "Click: campo");
        Oled.drawStr(0, 63, DeskBus::ready(DeskBus::Device::DS3231) ? "ESC: guardar" : "RTC no detectado");
        Oled.sendBuffer();
    }

    void drawRtcEditDate()
    {
        if (!rtcEditLoaded)
        {
            loadRtcEdit();
        }

        char day[4];
        char month[4];
        char year[6];
        snprintf(day, sizeof(day), "%02u", rtcEditDay);
        snprintf(month, sizeof(month), "%02u", rtcEditMonth);
        snprintf(year, sizeof(year), "%04u", rtcEditYear);

        Oled.clearBuffer();
        drawTitle("Ajustar Fecha");
        drawRtcField(0, 5, 34, "DD", day);
        drawRtcField(1, 46, 34, "MM", month);
        drawRtcField(2, 87, 34, "YYYY", year);
        Oled.setFont(OledFont::Small);
        Oled.drawStr(0, 54, "Click: campo");
        Oled.drawStr(0, 63, DeskBus::ready(DeskBus::Device::DS3231) ? "ESC: guardar" : "RTC no detectado");
        Oled.sendBuffer();
    }

    void drawNotificationMode()
    {
        Oled.clearBuffer();
        drawTitle("Notify mode");
        Oled.setFont(OledFont::Value);
        Oled.setCursor(8, 38);
        Oled.print(NotificationPixels::mode());
        Oled.setFont(OledFont::Small);
        Oled.drawStr(0, 62, "turn: change  ESC: back");
        Oled.sendBuffer();
    }

    void scanI2c()
    {
        scannedAddressCount = DeskBus::scanSensors(scannedAddresses, sizeof(scannedAddresses));
    }

    void scanSystemI2c()
    {
        scannedAddressCount = DeskBus::scanSystem(scannedAddresses, sizeof(scannedAddresses));
    }

    void goBack()
    {
        switch (activeScreen)
        {
            case Screen::Home:
                break;
            case Screen::MainMenu:
                activeScreen = Screen::Home;
                break;
            case Screen::UsbInfo:
            case Screen::LightMenu:
            case Screen::BusMenu:
            case Screen::RtcMenu:
            case Screen::SystemMenu:
                activeScreen = Screen::MainMenu;
                break;
            case Screen::LightEnabled:
            case Screen::LightMode:
            case Screen::LightBrightnessMenu:
            case Screen::LightKelvinMenu:
            case Screen::LightButtonsMenu:
                activeScreen = Screen::LightMenu;
                break;
            case Screen::LightBrightnessValue:
            case Screen::LightBrightnessMin:
            case Screen::LightBrightnessMax:
                activeScreen = Screen::LightBrightnessMenu;
                break;
            case Screen::LightKelvinValue:
            case Screen::LightKelvinColdMin:
            case Screen::LightKelvinColdMax:
            case Screen::LightKelvinHotMin:
            case Screen::LightKelvinHotMax:
                activeScreen = Screen::LightKelvinMenu;
                break;
            case Screen::LightButtonClickTime:
            case Screen::LightButtonLongTime:
            case Screen::LightButtonDuringTime:
            case Screen::LightBrightnessStep:
            case Screen::LightKelvinStep:
                activeScreen = Screen::LightButtonsMenu;
                break;
            case Screen::BusScan:
            case Screen::BusSensorsMenu:
            case Screen::BusDeviceStatus:
                activeScreen = Screen::BusMenu;
                break;
            case Screen::RtcSetTime:
            case Screen::RtcSetDate:
                saveRtcEdit();
                activeScreen = Screen::RtcMenu;
                break;
            case Screen::RtcAlarmsMenu:
                activeScreen = Screen::RtcMenu;
                break;
            case Screen::RtcAlarmList:
            case Screen::RtcCreateAlarm:
            case Screen::RtcAlarmMenu:
                activeScreen = Screen::RtcAlarmsMenu;
                break;
            case Screen::RtcAlarmEdit:
            case Screen::RtcAlarmNotification:
            case Screen::RtcAlarmDelete:
                activeScreen = Screen::RtcAlarmMenu;
                break;
            case Screen::DisplayTimeout:
            case Screen::SystemBusScan:
            case Screen::ChipInfo:
            case Screen::SystemNotificationsMenu:
            case Screen::SystemHomeViewMenu:
                activeScreen = Screen::SystemMenu;
                break;
            case Screen::SystemSensorsMenu:
                activeScreen = sensorConfigParent == ParentMenu::Bus ? Screen::BusMenu : Screen::SystemMenu;
                break;
            case Screen::TemperatureUnit:
            case Screen::SensorSampleInterval:
            case Screen::SensorSampleCount:
                activeScreen = Screen::SystemSensorsMenu;
                break;
            case Screen::SystemNotificationList:
            case Screen::SystemNotificationMenu:
                activeScreen = Screen::SystemNotificationsMenu;
                break;
            case Screen::SystemNotificationEdit:
            case Screen::SystemNotificationDelete:
                activeScreen = Screen::SystemNotificationMenu;
                break;
        }

        markInteraction();
    }

    void openSelected()
    {
        switch (activeScreen)
        {
            case Screen::Home:
                activeScreen = Screen::MainMenu;
                break;
            case Screen::MainMenu:
                switch (selectedMain)
                {
                    case MainUsbInfo:
                        activeScreen = Screen::UsbInfo;
                        break;
                    case MainLight:
                        activeScreen = Screen::LightMenu;
                        break;
                    case MainBus:
                        activeScreen = Screen::BusMenu;
                        break;
                    case MainRtc:
                        activeScreen = Screen::RtcMenu;
                        break;
                    case MainSystem:
                        activeScreen = Screen::SystemMenu;
                        break;
                    default:
                        break;
                }
                break;
            case Screen::LightMenu:
                if (selectedLight == LightBack)
                {
                    activeScreen = Screen::MainMenu;
                }
                else if (selectedLight == LightEnabledOption)
                {
                    activeScreen = Screen::LightEnabled;
                }
                else if (selectedLight == LightModeOption)
                {
                    activeScreen = Screen::LightMode;
                }
                else if (selectedLight == LightBrightnessOption)
                {
                    activeScreen = Screen::LightBrightnessMenu;
                }
                else if (selectedLight == LightKelvinOption)
                {
                    activeScreen = Screen::LightKelvinMenu;
                }
                else
                {
                    activeScreen = Screen::LightButtonsMenu;
                }
                break;
            case Screen::LightBrightnessMenu:
                if (selectedLightBrightness == LightBrightnessBack)
                {
                    activeScreen = Screen::LightMenu;
                }
                else
                {
                    activeScreen = static_cast<Screen>(static_cast<uint8_t>(Screen::LightBrightnessValue) + selectedLightBrightness);
                }
                break;
            case Screen::LightKelvinMenu:
                if (selectedLightKelvin == LightKelvinBack)
                {
                    activeScreen = Screen::LightMenu;
                }
                else
                {
                    activeScreen = static_cast<Screen>(static_cast<uint8_t>(Screen::LightKelvinValue) + selectedLightKelvin);
                }
                break;
            case Screen::LightButtonsMenu:
                if (selectedLightButton == LightButtonBack)
                {
                    activeScreen = Screen::LightMenu;
                }
                else
                {
                    activeScreen = static_cast<Screen>(static_cast<uint8_t>(Screen::LightButtonClickTime) + selectedLightButton);
                }
                break;
            case Screen::BusMenu:
                if (selectedBus == BusBack)
                {
                    activeScreen = Screen::MainMenu;
                }
                else if (selectedBus == BusScanOption)
                {
                    scanI2c();
                    activeScreen = Screen::BusScan;
                }
                else if (selectedBus == BusSensorsOption)
                {
                    activeScreen = Screen::BusSensorsMenu;
                }
                else if (selectedBus == BusSettingsOption)
                {
                    sensorConfigParent = ParentMenu::Bus;
                    activeScreen = Screen::SystemSensorsMenu;
                }
                break;
            case Screen::BusSensorsMenu:
                if (selectedBusSensor == BusSensorBack)
                {
                    activeScreen = Screen::BusMenu;
                }
                else
                {
                    activeScreen = Screen::BusDeviceStatus;
                }
                break;
            case Screen::RtcMenu:
                if (selectedRtc == RtcBack)
                {
                    activeScreen = Screen::MainMenu;
                }
                else if (selectedRtc == RtcAdjustTime)
                {
                    openRtcEdit(Screen::RtcSetTime);
                }
                else if (selectedRtc == RtcAdjustDate)
                {
                    openRtcEdit(Screen::RtcSetDate);
                }
                else
                {
                    activeScreen = Screen::RtcAlarmsMenu;
                }
                break;
            case Screen::RtcSetTime:
            case Screen::RtcSetDate:
                nextRtcEditField();
                break;
            case Screen::RtcAlarmsMenu:
                if (selectedRtcAlarm == RtcAlarmBack)
                {
                    activeScreen = Screen::RtcMenu;
                }
                else if (selectedRtcAlarm == RtcAlarmListOption)
                {
                    activeScreen = Screen::RtcAlarmList;
                }
                else
                {
                    activeScreen = Screen::RtcCreateAlarm;
                }
                break;
            case Screen::RtcAlarmList:
                activeScreen = Screen::RtcAlarmMenu;
                break;
            case Screen::RtcAlarmMenu:
                if (selectedRtcAlarmEdit == RtcAlarmEditBack)
                {
                    activeScreen = Screen::RtcAlarmsMenu;
                }
                else if (selectedRtcAlarmEdit == RtcAlarmEditOptionItem)
                {
                    activeScreen = Screen::RtcAlarmEdit;
                }
                else if (selectedRtcAlarmEdit == RtcAlarmNotificationOption)
                {
                    activeScreen = Screen::RtcAlarmNotification;
                }
                else
                {
                    activeScreen = Screen::RtcAlarmDelete;
                }
                break;
            case Screen::SystemMenu:
                if (selectedSystem == SystemBack)
                {
                    activeScreen = Screen::MainMenu;
                }
                else if (selectedSystem == SystemDisplayTimeout)
                {
                    activeScreen = Screen::DisplayTimeout;
                }
                else if (selectedSystem == SystemScanBus)
                {
                    scanSystemI2c();
                    activeScreen = Screen::SystemBusScan;
                }
                else if (selectedSystem == SystemChipInfo)
                {
                    activeScreen = Screen::ChipInfo;
                }
                else if (selectedSystem == SystemNotifications)
                {
                    activeScreen = Screen::SystemNotificationsMenu;
                }
                else
                {
                    activeScreen = Screen::SystemHomeViewMenu;
                }
                break;
            case Screen::SystemSensorsMenu:
                if (selectedSystemSensor == SystemSensorBack)
                {
                    activeScreen = sensorConfigParent == ParentMenu::Bus ? Screen::BusMenu : Screen::SystemMenu;
                }
                else if (selectedSystemSensor == SystemSensorSampleCount)
                {
                    activeScreen = Screen::SensorSampleCount;
                }
                else if (selectedSystemSensor == SystemSensorUnits)
                {
                    activeScreen = Screen::TemperatureUnit;
                }
                else
                {
                    activeScreen = Screen::SensorSampleInterval;
                }
                break;
            case Screen::SystemNotificationsMenu:
                if (selectedNotification == SystemNotificationBack)
                {
                    activeScreen = Screen::SystemMenu;
                }
                else
                {
                    activeScreen = Screen::SystemNotificationMenu;
                }
                break;
            case Screen::SystemNotificationMenu:
                if (selectedNotificationEdit == SystemNotificationEditBack)
                {
                    activeScreen = Screen::SystemNotificationsMenu;
                }
                else if (selectedNotificationEdit == SystemNotificationEditOptionItem)
                {
                    activeScreen = Screen::SystemNotificationEdit;
                }
                else
                {
                    activeScreen = Screen::SystemNotificationDelete;
                }
                break;
            case Screen::SystemHomeViewMenu:
                if (selectedHomeView == HomeViewBack)
                {
                    activeScreen = Screen::SystemMenu;
                }
                else
                {
                    moveSelectedHomeField(1);
                }
                break;
            case Screen::LightEnabled:
                StripLight::toggleEnabled();
                markStripConfigChanged();
                break;
            case Screen::LightMode:
                StripLight::toggleMode();
                markStripConfigChanged();
                break;
            case Screen::TemperatureUnit:
                DeskSettings::data().temperatureUnit = DeskSettings::data().temperatureUnit == 0 ? 1 : 0;
                DeskSettings::markDirty();
                break;
            case Screen::BusScan:
                scanI2c();
                break;
            case Screen::SystemBusScan:
                scanSystemI2c();
                break;
            default:
                break;
        }

        markInteraction();
    }

    void handleTurn(int16_t delta)
    {
        switch (activeScreen)
        {
            case Screen::Home:
                activeScreen = Screen::MainMenu;
                break;
            case Screen::MainMenu:
                moveSelection(selectedMain, MainOptionCount, delta);
                break;
            case Screen::LightMenu:
                moveSelection(selectedLight, LightOptionCount, delta);
                break;
            case Screen::LightBrightnessMenu:
                moveSelection(selectedLightBrightness, LightBrightnessOptionCount, delta);
                break;
            case Screen::LightKelvinMenu:
                moveSelection(selectedLightKelvin, LightKelvinOptionCount, delta);
                break;
            case Screen::LightButtonsMenu:
                moveSelection(selectedLightButton, LightButtonOptionCount, delta);
                break;
            case Screen::BusMenu:
                moveSelection(selectedBus, BusOptionCount, delta);
                break;
            case Screen::BusSensorsMenu:
                moveSelection(selectedBusSensor, BusSensorOptionCount, delta);
                break;
            case Screen::RtcMenu:
                moveSelection(selectedRtc, RtcOptionCount, delta);
                break;
            case Screen::RtcSetTime:
            case Screen::RtcSetDate:
                moveRtcEdit(delta);
                break;
            case Screen::RtcAlarmsMenu:
                moveSelection(selectedRtcAlarm, RtcAlarmOptionCount, delta);
                break;
            case Screen::RtcAlarmMenu:
                moveSelection(selectedRtcAlarmEdit, RtcAlarmEditOptionCount, delta);
                break;
            case Screen::SystemMenu:
                moveSelection(selectedSystem, SystemOptionCount, delta);
                break;
            case Screen::SystemSensorsMenu:
                moveSelection(selectedSystemSensor, SystemSensorOptionCount, delta);
                break;
            case Screen::SystemNotificationsMenu:
                moveSelection(selectedNotification, SystemNotificationOptionCount, delta);
                break;
            case Screen::SystemNotificationMenu:
                moveSelection(selectedNotificationEdit, SystemNotificationEditOptionCount, delta);
                break;
            case Screen::SystemHomeViewMenu:
                moveSelection(selectedHomeView, HomeViewOptionCount, delta);
                break;
            case Screen::LightBrightnessValue:
                StripLight::setBrightness(adjustedValue(StripLight::brightness(), delta, 16, STRIP_BRIGHTNESS_MIN_DEFAULT, STRIP_PWM_MAX_DEFAULT));
                StripLight::setEnabled(true);
                markStripConfigChanged();
                break;
            case Screen::LightBrightnessMin:
                StripLight::setBrightnessMin(adjustedValue(StripLight::brightnessMin(), delta, 16, STRIP_BRIGHTNESS_MIN_DEFAULT, STRIP_PWM_MAX_DEFAULT));
                markStripConfigChanged();
                break;
            case Screen::LightBrightnessMax:
                StripLight::setBrightnessMax(adjustedValue(StripLight::brightnessMax(), delta, 16, STRIP_BRIGHTNESS_MIN_DEFAULT, STRIP_PWM_MAX_DEFAULT));
                markStripConfigChanged();
                break;
            case Screen::LightKelvinValue:
                StripLight::setKelvinMix(adjustedValue(StripLight::kelvinMix(), delta, 16, STRIP_HOT_MIN_DEFAULT, STRIP_PWM_MAX_DEFAULT));
                StripLight::setEnabled(true);
                markStripConfigChanged();
                break;
            case Screen::LightKelvinColdMin:
                StripLight::setColdMin(adjustedValue(StripLight::coldMin(), delta, 16, STRIP_COLD_MIN_DEFAULT, STRIP_PWM_MAX_DEFAULT));
                markStripConfigChanged();
                break;
            case Screen::LightKelvinColdMax:
                StripLight::setColdMax(adjustedValue(StripLight::coldMax(), delta, 16, STRIP_COLD_MIN_DEFAULT, STRIP_PWM_MAX_DEFAULT));
                markStripConfigChanged();
                break;
            case Screen::LightKelvinHotMin:
                StripLight::setHotMin(adjustedValue(StripLight::hotMin(), delta, 16, STRIP_HOT_MIN_DEFAULT, STRIP_PWM_MAX_DEFAULT));
                markStripConfigChanged();
                break;
            case Screen::LightKelvinHotMax:
                StripLight::setHotMax(adjustedValue(StripLight::hotMax(), delta, 16, STRIP_HOT_MIN_DEFAULT, STRIP_PWM_MAX_DEFAULT));
                markStripConfigChanged();
                break;
            case Screen::LightButtonClickTime:
                StripLight::setButtonClickTime(adjustedValue(StripLight::buttonClickTime(), delta, 50, STRIP_BUTTON_CLICK_MS_MIN_DEFAULT, STRIP_BUTTON_CLICK_MS_MAX_DEFAULT));
                markStripConfigChanged();
                break;
            case Screen::LightButtonLongTime:
                StripLight::setButtonLongTime(adjustedValue(StripLight::buttonLongTime(), delta, 50, STRIP_BUTTON_LONG_MS_MIN_DEFAULT, STRIP_BUTTON_LONG_MS_MAX_DEFAULT));
                markStripConfigChanged();
                break;
            case Screen::LightButtonDuringTime:
                StripLight::setButtonDuringTime(adjustedValue(StripLight::buttonDuringTime(), delta, 10, STRIP_BUTTON_REPEAT_MS_MIN_DEFAULT, STRIP_BUTTON_REPEAT_MS_MAX_DEFAULT));
                markStripConfigChanged();
                break;
            case Screen::LightBrightnessStep:
                StripLight::setBrightnessStep(adjustedValue(StripLight::brightnessStep(), delta, 1, STRIP_STEP_MIN_DEFAULT, STRIP_STEP_MAX_DEFAULT));
                markStripConfigChanged();
                break;
            case Screen::LightKelvinStep:
                StripLight::setKelvinStep(adjustedValue(StripLight::kelvinStep(), delta, 1, STRIP_STEP_MIN_DEFAULT, STRIP_STEP_MAX_DEFAULT));
                markStripConfigChanged();
                break;
            case Screen::DisplayTimeout:
                DeskSettings::data().displayTimeoutMs = adjustedValue(DeskSettings::data().displayTimeoutMs, delta, DISPLAY_TIMEOUT_MS_STEP_DEFAULT, DISPLAY_TIMEOUT_MS_MIN_DEFAULT, DISPLAY_TIMEOUT_MS_MAX_DEFAULT);
                DeskSettings::markDirty();
                break;
            case Screen::SensorSampleInterval:
                DeskSettings::data().sensorSampleIntervalMs = adjustedValue(DeskSettings::data().sensorSampleIntervalMs, delta, SENSOR_SAMPLE_INTERVAL_MS_STEP_DEFAULT, SENSOR_SAMPLE_INTERVAL_MS_MIN_DEFAULT, SENSOR_SAMPLE_INTERVAL_MS_MAX_DEFAULT);
                DeskSettings::markDirty();
                break;
            case Screen::SensorSampleCount:
                DeskSettings::data().sensorSampleCount = adjustedValue(DeskSettings::data().sensorSampleCount, delta, 1, SENSOR_SAMPLE_COUNT_MIN_DEFAULT, SENSOR_SAMPLE_COUNT_MAX_DEFAULT);
                DeskSettings::markDirty();
                break;
            default:
                break;
        }

        markInteraction();
    }

    void drawActiveScreen()
    {
        switch (activeScreen)
        {
            case Screen::Home:
                drawHome();
                break;
            case Screen::MainMenu:
                drawOptionList("Menu", mainOptions, MainOptionCount, selectedMain);
                break;
            case Screen::UsbInfo:
                drawUsbInfo();
                break;
            case Screen::LightMenu:
                drawOptionList("Light", lightOptions, LightOptionCount, selectedLight);
                break;
            case Screen::LightBrightnessMenu:
                drawOptionList("Brightness", lightBrightnessOptions, LightBrightnessOptionCount, selectedLightBrightness);
                break;
            case Screen::LightKelvinMenu:
                drawOptionList("Kelvin", lightKelvinOptions, LightKelvinOptionCount, selectedLightKelvin);
                break;
            case Screen::LightButtonsMenu:
                drawOptionList("Light buttons", lightButtonOptions, LightButtonOptionCount, selectedLightButton);
                break;
            case Screen::BusMenu:
                drawBusMenu();
                break;
            case Screen::BusSensorsMenu:
                drawBusSensorsMenu();
                break;
            case Screen::RtcMenu:
                drawOptionList("RTC", rtcOptions, RtcOptionCount, selectedRtc);
                break;
            case Screen::RtcAlarmsMenu:
                drawOptionList("RTC alarms", rtcAlarmOptions, RtcAlarmOptionCount, selectedRtcAlarm);
                break;
            case Screen::RtcAlarmMenu:
                drawOptionList("Alarm", rtcAlarmEditOptions, RtcAlarmEditOptionCount, selectedRtcAlarmEdit);
                break;
            case Screen::SystemMenu:
                drawOptionList("System", systemOptions, SystemOptionCount, selectedSystem);
                break;
            case Screen::ChipInfo:
                drawChipInfo();
                break;
            case Screen::SystemSensorsMenu:
                drawOptionList("Bus settings", systemSensorOptions, SystemSensorOptionCount, selectedSystemSensor);
                break;
            case Screen::SystemNotificationsMenu:
                drawOptionList("Notifications", notificationOptions, SystemNotificationOptionCount, selectedNotification);
                break;
            case Screen::SystemNotificationMenu:
                drawOptionList("Notify item", notificationEditOptions, SystemNotificationEditOptionCount, selectedNotificationEdit);
                break;
            case Screen::SystemHomeViewMenu:
                drawHomeViewMenu();
                break;
            case Screen::LightEnabled:
                drawEnabledEdit();
                break;
            case Screen::LightMode:
                drawModeEdit();
                break;
            case Screen::LightBrightnessValue:
                drawRawEdit("Brightness", StripLight::brightness(), "turn: 0..1023");
                break;
            case Screen::LightBrightnessMin:
                drawRawEdit("Brightness min", StripLight::brightnessMin(), "turn: 0..1023");
                break;
            case Screen::LightBrightnessMax:
                drawRawEdit("Brightness max", StripLight::brightnessMax(), "turn: 0..1023");
                break;
            case Screen::LightKelvinValue:
                drawRawEdit("Kelvin", StripLight::kelvinMix(), "0 warm, 1023 cold");
                break;
            case Screen::LightKelvinColdMin:
                drawRawEdit("Cold min", StripLight::coldMin(), "PWM minimum");
                break;
            case Screen::LightKelvinColdMax:
                drawRawEdit("Cold max", StripLight::coldMax(), "PWM maximum");
                break;
            case Screen::LightKelvinHotMin:
                drawRawEdit("Hot min", StripLight::hotMin(), "PWM minimum");
                break;
            case Screen::LightKelvinHotMax:
                drawRawEdit("Hot max", StripLight::hotMax(), "PWM maximum");
                break;
            case Screen::LightButtonClickTime:
                drawRawEdit("Click time", StripLight::buttonClickTime(), "milliseconds");
                break;
            case Screen::LightButtonLongTime:
                drawRawEdit("Long time", StripLight::buttonLongTime(), "milliseconds");
                break;
            case Screen::LightButtonDuringTime:
                drawRawEdit("During time", StripLight::buttonDuringTime(), "repeat ms");
                break;
            case Screen::LightBrightnessStep:
                drawRawEdit("Brightness step", StripLight::brightnessStep(), "button hold step");
                break;
            case Screen::LightKelvinStep:
                drawRawEdit("Kelvin step", StripLight::kelvinStep(), "button hold step");
                break;
            case Screen::BusScan:
                drawI2cScan("Sensor scan");
                break;
            case Screen::SystemBusScan:
                drawI2cScan("System scan");
                break;
            case Screen::BusDeviceStatus:
                drawBusDeviceStatus(selectedBusSensor);
                break;
            case Screen::RtcSetTime:
                drawRtcEditTime();
                break;
            case Screen::RtcSetDate:
                drawRtcEditDate();
                break;
            case Screen::RtcAlarmList:
                drawPlaceholder("Alarmas", "No alarms stored");
                break;
            case Screen::RtcCreateAlarm:
                drawPlaceholder("Crear Alarma", "Pending form");
                break;
            case Screen::RtcAlarmEdit:
                drawPlaceholder("Editar Alarma", "Pending editor");
                break;
            case Screen::RtcAlarmNotification:
                drawPlaceholder("Vincular Notify", "Pending selector");
                break;
            case Screen::RtcAlarmDelete:
                drawPlaceholder("Eliminar Alarma", "Pending confirm");
                break;
            case Screen::DisplayTimeout:
                drawRawEdit("Display timeout", DeskSettings::data().displayTimeoutMs / 1000, "seconds");
                break;
            case Screen::TemperatureUnit:
                drawTemperatureUnitEdit();
                break;
            case Screen::SensorSampleInterval:
                drawSecondsEdit("Sensor sample", DeskSettings::data().sensorSampleIntervalMs, "range 01.0..30.0");
                break;
            case Screen::SensorSampleCount:
                drawRawEdit("Samples count", DeskSettings::data().sensorSampleCount, "average samples");
                break;
            case Screen::SystemNotificationList:
            case Screen::SystemNotificationEdit:
                drawPlaceholder("Editar Notify", notificationOptions[selectedNotification], "Color RGB pending");
                break;
            case Screen::SystemNotificationDelete:
                drawPlaceholder("Eliminar Notify", notificationOptions[selectedNotification], "Pending confirm");
                break;
        }
    }
}

namespace OledPanel
{
    void begin()
    {
        Oled.begin();
        lastInteractionMs = millis();
    }

    void bootBegin()
    {
        bootLineCount = 0;
        bootStartMs = millis();
        drawBoot();
    }

    void update()
    {
        const int16_t encoderDelta = PanelControls::consumeEncoderDelta();
        const PanelControls::Event event = PanelControls::consumeEvent();

        if (encoderDelta != 0)
        {
            handleTurn(encoderDelta);
        }

        if (event == PanelControls::Event::EncoderClick)
        {
            openSelected();
        }
        else if (event == PanelControls::Event::EscapeClick)
        {
            goBack();
        }

        const uint32_t now = millis();
        if (activeScreen != Screen::Home && now - lastInteractionMs >= DeskSettings::data().displayTimeoutMs)
        {
            if (activeScreen == Screen::RtcSetTime || activeScreen == Screen::RtcSetDate)
            {
                saveRtcEdit();
            }

            activeScreen = Screen::Home;
            needsRedraw = true;
        }

        const uint32_t redrawIntervalMs = periodicRedrawInterval();
        if (needsRedraw || (redrawIntervalMs > 0 && now - lastDrawMs >= redrawIntervalMs))
        {
            lastDrawMs = now;
            needsRedraw = false;
            drawActiveScreen();
        }
    }

    void showBootStatus(const char *line)
    {
        if (bootLineCount < 3)
        {
            bootLines[bootLineCount++] = line;
        }
        else
        {
            bootLines[0] = bootLines[1];
            bootLines[1] = bootLines[2];
            bootLines[2] = line;
        }

        drawBoot();
        animateBootFor(170);
    }

    void bootFinish()
    {
        while (millis() - bootStartMs < bootDurationMs)
        {
            DeskUSB::task();
            drawBoot();
            delay(DISPLAY_BOOT_FRAME_MS_DEFAULT);
        }

        DeskUSB::task();
        drawBoot();
        NotificationPixels::clear();
        delay(DISPLAY_BOOT_FINISH_DELAY_MS_DEFAULT);
        lastInteractionMs = millis();
        needsRedraw = true;
        drawActiveScreen();
    }
}
