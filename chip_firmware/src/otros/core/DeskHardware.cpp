#include "core/DeskHardware.h"

#include <Arduino.h>

#include "bus/DeskBus.h"
#include "core/DeskSettings.h"
#include "core/McuMonitor.h"
#include "core/StatusIndicator.h"
#include "input/PanelControls.h"
#include "light/NotificationPixels.h"
#include "light/StripLight.h"
#include "ui/OledPanel.h"
#include "usb/DeskUSB.h"

namespace DeskHardware
{
    void begin()
    {
        StatusIndicator::begin();
        StripLight::begin();
        NotificationPixels::begin();
        PanelControls::begin();
        McuMonitor::begin();
        OledPanel::begin();
        OledPanel::bootBegin();

        OledPanel::showBootStatus(DeskUSB::mounted() ? "USB=OK" : "USB=WAIT");
        OledPanel::showBootStatus("SPI=OK");
        OledPanel::showBootStatus("OLED=OK");
        OledPanel::showBootStatus("NeoPixel=OK");

        DeskBus::begin();
        OledPanel::showBootStatus("Sys PB7/PB6");
        OledPanel::showBootStatus("Sens PB8/PA8");
        OledPanel::showBootStatus(DeskBus::ready(DeskBus::Device::DS3231) ? "DS3231=OK" : "DS3231=MISS");
        OledPanel::showBootStatus(DeskBus::ready(DeskBus::Device::AT24C32) ? "AT24C32=OK" : "AT24C32=MISS");
        OledPanel::showBootStatus(DeskBus::ready(DeskBus::Device::ENS160) ? "ENS160=OK" : "ENS160=MISS");
        OledPanel::showBootStatus(DeskBus::ready(DeskBus::Device::AHT21) ? "AHT21=OK" : "AHT21=MISS");
        OledPanel::showBootStatus(DeskBus::ready(DeskBus::Device::VEML7700) ? "VEML7700=OK" : "VEML7700=MISS");
        OledPanel::showBootStatus(DeskBus::ready(DeskBus::Device::SCD41) ? "SCD41=OK" : "SCD41=MISS");
        OledPanel::showBootStatus(DeskBus::ready(DeskBus::Device::SHT21) ? "SHT21=OK" : "SHT21=MISS");
        DeskSettings::begin();
        OledPanel::showBootStatus(DeskSettings::storageReady() ? "EEPROM CFG=OK" : "EEPROM CFG=RAM");
        OledPanel::bootFinish();
    }

    void update()
    {
        StripLight::update();
        PanelControls::update();
        OledPanel::update();
        DeskBus::update();
        DeskSettings::update();
        McuMonitor::update();
        StatusIndicator::update();
    }
}
