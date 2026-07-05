#include "Antenna.hpp"

#include <string.h>

#if defined(ARDUINO_ARCH_ESP32)
    #include <Arduino.h>
    #include <BLE2902.h>
    #include <BLEDevice.h>
    #include <WiFi.h>
#endif

Antenna WirelessAntenna;

#if defined(ARDUINO_ARCH_ESP32)
namespace
{
    BLEServer *bleServer = nullptr;
    BLECharacteristic *bleCharacteristic = nullptr;

    class DeskBleServerCallbacks : public BLEServerCallbacks
    {
    public:
        void onConnect(BLEServer *) override
        {
            WirelessAntenna.onBleConnected_();
        }

        void onDisconnect(BLEServer *) override
        {
            WirelessAntenna.onBleDisconnected_();
        }
    };

    class DeskBleCharacteristicCallbacks : public BLECharacteristicCallbacks
    {
    public:
        void onWrite(BLECharacteristic *characteristic) override
        {
            std::string value = characteristic->getValue();
            WirelessAntenna.onBleWrite_(value.c_str());
        }
    };

    DeskBleServerCallbacks serverCallbacks;
    DeskBleCharacteristicCallbacks characteristicCallbacks;
}
#endif

void Antenna::begin()
{
    pendingChanges_ = true;
    state_ = State::Stopped;
}

void Antenna::update()
{
#if defined(ARDUINO_ARCH_ESP32)
    if (mode_ == Mode::WifiClient && enabled_)
    {
        state_ = WiFi.status() == WL_CONNECTED ? State::WifiConnected : State::WifiConnecting;
    }
    else if (mode_ == Mode::BleDevice && enabled_)
    {
        state_ = bleConnected_ ? State::BleConnected : State::BleAdvertising;
    }
#endif
}

bool Antenna::apply()
{
    stopWireless();
    pendingChanges_ = false;

    if (!enabled_ || mode_ == Mode::Off)
    {
        state_ = State::Off;
        return true;
    }

    switch (mode_)
    {
        case Mode::BleDevice:
            return startBleDevice();
        case Mode::WifiClient:
            return startWifiClient();
        case Mode::WifiAp:
            return startWifiAp();
        case Mode::Off:
        default:
            state_ = State::Off;
            return true;
    }
}

void Antenna::stop()
{
    enabled_ = false;
    mode_ = Mode::Off;
    stopWireless();
    state_ = State::Off;
    pendingChanges_ = false;
}

void Antenna::setEnabled(bool value)
{
    if (enabled_ == value)
    {
        return;
    }

    enabled_ = value;
    markDirty();
}

bool Antenna::enabled() const
{
    return enabled_;
}

void Antenna::setMode(Mode value)
{
    if (mode_ == value)
    {
        return;
    }

    mode_ = value;
    markDirty();
}

Antenna::Mode Antenna::mode() const
{
    return mode_;
}

void Antenna::setBleConfig(const BleConfig &config)
{
    copyText(ble_.deviceName, sizeof(ble_.deviceName), config.deviceName);
    copyText(ble_.serviceUuid, sizeof(ble_.serviceUuid), config.serviceUuid);
    copyText(ble_.characteristicUuid, sizeof(ble_.characteristicUuid), config.characteristicUuid);
    copyText(ble_.characteristicValue, sizeof(ble_.characteristicValue), config.characteristicValue);
    markDirty();
}

const Antenna::BleConfig &Antenna::bleConfig() const
{
    return ble_;
}

void Antenna::setWifiClientConfig(const WifiClientConfig &config)
{
    copyText(wifiClient_.ssid, sizeof(wifiClient_.ssid), config.ssid);
    copyText(wifiClient_.password, sizeof(wifiClient_.password), config.password);
    wifiClient_.connectTimeoutMs = config.connectTimeoutMs;
    wifiClient_.autoReconnect = config.autoReconnect;
    markDirty();
}

const Antenna::WifiClientConfig &Antenna::wifiClientConfig() const
{
    return wifiClient_;
}

void Antenna::setWifiApConfig(const WifiApConfig &config)
{
    copyText(wifiAp_.ssid, sizeof(wifiAp_.ssid), config.ssid);
    copyText(wifiAp_.password, sizeof(wifiAp_.password), config.password);
    wifiAp_.channel = config.channel;
    wifiAp_.hidden = config.hidden;
    wifiAp_.maxConnections = config.maxConnections;
    markDirty();
}

const Antenna::WifiApConfig &Antenna::wifiApConfig() const
{
    return wifiAp_;
}

bool Antenna::pendingChanges() const
{
    return pendingChanges_;
}

void Antenna::markDirty()
{
    pendingChanges_ = true;
}

Antenna::State Antenna::state() const
{
    return state_;
}

const char *Antenna::stateText() const
{
    switch (state_)
    {
        case State::Off:
            return "off";
        case State::Stopped:
            return "stopped";
        case State::BleAdvertising:
            return "ble_advertising";
        case State::BleConnected:
            return "ble_connected";
        case State::WifiConnecting:
            return "wifi_connecting";
        case State::WifiConnected:
            return "wifi_connected";
        case State::WifiApRunning:
            return "wifi_ap_running";
        case State::Error:
            return "error";
        case State::Unsupported:
            return "unsupported";
        default:
            return "unknown";
    }
}

const char *Antenna::modeText() const
{
    switch (mode_)
    {
        case Mode::Off:
            return "off";
        case Mode::BleDevice:
            return "ble_device";
        case Mode::WifiClient:
            return "wifi_client";
        case Mode::WifiAp:
            return "wifi_ap";
        default:
            return "unknown";
    }
}

bool Antenna::bleConnected() const
{
    return bleConnected_;
}

uint32_t Antenna::bleConnectionCount() const
{
    return bleConnectionCount_;
}

bool Antenna::bleRxPending() const
{
    return bleRxPending_;
}

const char *Antenna::bleLastRx() const
{
    return bleLastRx_;
}

void Antenna::clearBleRx()
{
    bleRxPending_ = false;
    bleLastRx_[0] = '\0';
}

bool Antenna::bleWrite(const char *text)
{
#if defined(ARDUINO_ARCH_ESP32)
    if (bleCharacteristic == nullptr || text == nullptr)
    {
        return false;
    }

    bleCharacteristic->setValue(text);
    if (bleConnected_)
    {
        bleCharacteristic->notify();
    }
    return true;
#else
    (void)text;
    return false;
#endif
}

bool Antenna::startBleDevice()
{
#if defined(ARDUINO_ARCH_ESP32)
    BLEDevice::init(ble_.deviceName);

    bleServer = BLEDevice::createServer();
    bleServer->setCallbacks(&serverCallbacks);
    BLEService *service = bleServer->createService(ble_.serviceUuid);
    bleCharacteristic = service->createCharacteristic(
        ble_.characteristicUuid,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_NOTIFY);

    bleCharacteristic->setCallbacks(&characteristicCallbacks);
    bleCharacteristic->setValue(ble_.characteristicValue);
    bleCharacteristic->addDescriptor(new BLE2902());
    service->start();

    BLEAdvertising *advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(ble_.serviceUuid);
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06);
    advertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    state_ = State::BleAdvertising;
    bleConnected_ = false;
    return true;
#else
    state_ = State::Unsupported;
    return false;
#endif
}

bool Antenna::startWifiClient()
{
#if defined(ARDUINO_ARCH_ESP32)
    if (wifiClient_.ssid[0] == '\0')
    {
        state_ = State::Error;
        return false;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(wifiClient_.autoReconnect);
    WiFi.begin(wifiClient_.ssid, wifiClient_.password);

    state_ = State::WifiConnecting;
    const uint32_t startMs = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startMs < wifiClient_.connectTimeoutMs)
    {
        delay(10);
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        state_ = State::WifiConnected;
        return true;
    }

    state_ = State::Error;
    return false;
#else
    state_ = State::Unsupported;
    return false;
#endif
}

bool Antenna::startWifiAp()
{
#if defined(ARDUINO_ARCH_ESP32)
    if (wifiAp_.ssid[0] == '\0')
    {
        state_ = State::Error;
        return false;
    }

    WiFi.mode(WIFI_AP);
    const char *password = wifiAp_.password[0] == '\0' ? nullptr : wifiAp_.password;
    const bool started = WiFi.softAP(wifiAp_.ssid, password, wifiAp_.channel, wifiAp_.hidden, wifiAp_.maxConnections);

    state_ = started ? State::WifiApRunning : State::Error;
    return started;
#else
    state_ = State::Unsupported;
    return false;
#endif
}

void Antenna::stopWireless()
{
#if defined(ARDUINO_ARCH_ESP32)
    BLEDevice::deinit(true);
    bleServer = nullptr;
    bleCharacteristic = nullptr;
    WiFi.disconnect(true, true);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
#endif
    bleConnected_ = false;
    state_ = State::Stopped;
}

void Antenna::copyText(char *target, uint32_t targetSize, const char *source)
{
    if (target == nullptr || targetSize == 0)
    {
        return;
    }

    if (source == nullptr)
    {
        target[0] = '\0';
        return;
    }

    strncpy(target, source, targetSize - 1);
    target[targetSize - 1] = '\0';
}

void Antenna::onBleConnected_()
{
    bleConnected_ = true;
    ++bleConnectionCount_;
    state_ = State::BleConnected;
}

void Antenna::onBleDisconnected_()
{
    bleConnected_ = false;
    if (enabled_ && mode_ == Mode::BleDevice)
    {
        state_ = State::BleAdvertising;
#if defined(ARDUINO_ARCH_ESP32)
        BLEDevice::startAdvertising();
#endif
    }
}

void Antenna::onBleWrite_(const char *text)
{
    copyText(bleLastRx_, sizeof(bleLastRx_), text);
    bleRxPending_ = true;
}
