#pragma once

#include <stdint.h>

class Antenna
{
public:
    enum class Mode : uint8_t
    {
        Off = 0,
        BleDevice,
        WifiClient,
        WifiAp,
    };

    enum class State : uint8_t
    {
        Off = 0,
        Stopped,
        BleAdvertising,
        BleConnected,
        WifiConnecting,
        WifiConnected,
        WifiApRunning,
        Error,
        Unsupported,
    };

    struct BleConfig
    {
        char deviceName[32] = "DBx01";
        char serviceUuid[37] = "8f7f0001-6b21-4f5e-9c7e-123456780001";
        char characteristicUuid[37] = "8f7f0002-6b21-4f5e-9c7e-123456780001";
        char characteristicValue[64] = "DeskBridge";
    };

    struct WifiClientConfig
    {
        char ssid[33] = "";
        char password[65] = "";
        uint32_t connectTimeoutMs = 8000;
        bool autoReconnect = true;
    };

    struct WifiApConfig
    {
        char ssid[33] = "DBx01";
        char password[65] = "";
        uint8_t channel = 1;
        bool hidden = false;
        uint8_t maxConnections = 4;
    };

    void begin();
    void update();
    bool apply();
    void stop();

    void setEnabled(bool value);
    bool enabled() const;

    void setMode(Mode value);
    Mode mode() const;

    void setBleConfig(const BleConfig &config);
    const BleConfig &bleConfig() const;

    void setWifiClientConfig(const WifiClientConfig &config);
    const WifiClientConfig &wifiClientConfig() const;

    void setWifiApConfig(const WifiApConfig &config);
    const WifiApConfig &wifiApConfig() const;

    bool pendingChanges() const;
    void markDirty();

    State state() const;
    const char *stateText() const;
    const char *modeText() const;

    bool bleConnected() const;
    uint32_t bleConnectionCount() const;
    bool bleRxPending() const;
    const char *bleLastRx() const;
    void clearBleRx();
    bool bleWrite(const char *text);

    void onBleConnected_();
    void onBleDisconnected_();
    void onBleWrite_(const char *text);

private:
    bool startBleDevice();
    bool startWifiClient();
    bool startWifiAp();
    void stopWireless();
    static void copyText(char *target, uint32_t targetSize, const char *source);

    bool enabled_ = false;
    bool pendingChanges_ = true;
    bool bleConnected_ = false;
    bool bleRxPending_ = false;
    uint32_t bleConnectionCount_ = 0;
    Mode mode_ = Mode::Off;
    State state_ = State::Off;
    BleConfig ble_;
    WifiClientConfig wifiClient_;
    WifiApConfig wifiAp_;
    char bleLastRx_[96] = {};
};

extern Antenna WirelessAntenna;
