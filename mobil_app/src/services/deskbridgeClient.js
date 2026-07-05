import { PermissionsAndroid, Platform } from "react-native";

export const BLE_SERVICE_UUID = "8f7f0001-6b21-4f5e-9c7e-123456780001";
export const BLE_CHARACTERISTIC_UUID = "8f7f0002-6b21-4f5e-9c7e-123456780001";
export const BLE_DEVICE_NAME = "DBx01";

const MAX_COMMAND_LENGTH = 95;

const initialState = {
  connected: false,
  bleAvailable: true,
  connectionState: "idle",
  deviceName: BLE_DEVICE_NAME,
  firmware: "-",
  protocol: "-",
  productId: "-",
  uptimeS: null,
  updatedAt: "--:--:--",
  lastError: null,
  config: null,
  light: {
    enabled: false,
    mode: "-",
    brightness: 0,
    kelvin: 0,
    coldMin: null,
    coldMax: null,
    warmMin: null,
    warmMax: null
  },
  sensors: [
    {
      id: "environment",
      name: "Environment",
      status: "waiting",
      values: [
        { label: "Temp", value: "--", unit: "C" },
        { label: "Humidity", value: "--", unit: "%" },
        { label: "Light", value: "--", unit: "lux" }
      ]
    },
    {
      id: "air",
      name: "Air quality",
      status: "waiting",
      values: [
        { label: "CO2", value: "--", unit: "ppm" },
        { label: "TVOC", value: "--", unit: "ppb" },
        { label: "AQI", value: "--", unit: "" }
      ]
    }
  ]
};

const mockState = {
  ...initialState,
  bleAvailable: false,
  connectionState: "demo",
  firmware: "0.4.0",
  protocol: "0x04",
  productId: "demo",
  light: {
    ...initialState.light,
    enabled: true,
    mode: "composite",
    brightness: 32768,
    kelvin: 32768
  },
  sensors: [
    {
      id: "environment",
      name: "Environment",
      status: "ok",
      values: [
        { label: "Temp", value: "24.6", unit: "C" },
        { label: "Humidity", value: "48", unit: "%" },
        { label: "Light", value: "382", unit: "lux" }
      ]
    },
    {
      id: "air",
      name: "Air quality",
      status: "ok",
      values: [
        { label: "CO2", value: "418", unit: "ppm" },
        { label: "TVOC", value: "32", unit: "ppb" },
        { label: "AQI", value: "1", unit: "" }
      ]
    }
  ]
};

function cloneState(value) {
  return JSON.parse(JSON.stringify(value));
}

function nowLabel() {
  return new Date().toLocaleTimeString();
}

function clamp16(value) {
  const number = Number(value);
  if (!Number.isFinite(number)) return 0;
  return Math.max(0, Math.min(65535, Math.round(number)));
}

function parseBoolean(value) {
  return ["true", "on", "1", "yes"].includes(String(value).toLowerCase());
}

function parseFields(rawFields) {
  const fields = {};
  rawFields
    .split(",")
    .map((field) => field.trim())
    .filter(Boolean)
    .forEach((field) => {
      const separator = field.indexOf(":");
      if (separator === -1) return;
      const key = field.slice(0, separator).trim();
      const value = field.slice(separator + 1).trim();
      fields[key] = value;
    });
  return fields;
}

function updateSensorValues(state, fields) {
  const nextSensors = cloneState(state.sensors);
  const environment = nextSensors.find((sensor) => sensor.id === "environment");
  const air = nextSensors.find((sensor) => sensor.id === "air");

  if (environment) {
    environment.status = "ok";
    environment.values = [
      { label: "Temp", value: fields.temp_c ?? "--", unit: "C" },
      { label: "Humidity", value: fields.humidity_pct ?? "--", unit: "%" },
      { label: "Light", value: fields.lux ?? "--", unit: "lux" }
    ];
  }

  if (air) {
    air.status = "ok";
    air.values = [
      { label: "CO2", value: fields.co2_ppm ?? "--", unit: "ppm" },
      { label: "TVOC", value: fields.tvoc_ppb ?? "--", unit: "ppb" },
      { label: "AQI", value: fields.aqi ?? "--", unit: "" }
    ];
  }

  return nextSensors;
}

function asciiToBase64(input) {
  const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  let output = "";
  let index = 0;

  while (index < input.length) {
    const byte1 = input.charCodeAt(index++) & 0xff;
    const byte2 = index < input.length ? input.charCodeAt(index++) & 0xff : NaN;
    const byte3 = index < input.length ? input.charCodeAt(index++) & 0xff : NaN;

    output += chars[byte1 >> 2];
    output += chars[((byte1 & 3) << 4) | (Number.isNaN(byte2) ? 0 : byte2 >> 4)];
    output += Number.isNaN(byte2) ? "=" : chars[((byte2 & 15) << 2) | (Number.isNaN(byte3) ? 0 : byte3 >> 6)];
    output += Number.isNaN(byte3) ? "=" : chars[byte3 & 63];
  }

  return output;
}

function base64ToAscii(input) {
  const chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const clean = String(input || "").replace(/=+$/, "");
  let output = "";
  let buffer = 0;
  let bits = 0;

  for (const char of clean) {
    const value = chars.indexOf(char);
    if (value === -1) continue;
    buffer = (buffer << 6) | value;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      output += String.fromCharCode((buffer >> bits) & 0xff);
    }
  }

  return output;
}

function loadBleManager() {
  try {
    const { BleManager } = require("react-native-ble-plx");
    return new BleManager();
  } catch (error) {
    return null;
  }
}

async function requestBluetoothPermissions() {
  if (Platform.OS !== "android") return true;

  if (Platform.Version >= 31) {
    const grants = await PermissionsAndroid.requestMultiple([
      PermissionsAndroid.PERMISSIONS.BLUETOOTH_SCAN,
      PermissionsAndroid.PERMISSIONS.BLUETOOTH_CONNECT
    ]);
    return Object.values(grants).every((grant) => grant === PermissionsAndroid.RESULTS.GRANTED);
  }

  const grant = await PermissionsAndroid.request(PermissionsAndroid.PERMISSIONS.ACCESS_FINE_LOCATION);
  return grant === PermissionsAndroid.RESULTS.GRANTED;
}

class DeskBridgeBleClient {
  constructor() {
    this.manager = null;
    this.device = null;
    this.scanTimeout = null;
    this.notificationSubscription = null;
    this.disconnectSubscription = null;
    this.listeners = new Set();
    this.state = cloneState(initialState);
  }

  subscribe(listener) {
    this.listeners.add(listener);
    listener(this.snapshot());
    return () => this.listeners.delete(listener);
  }

  snapshot() {
    return cloneState(this.state);
  }

  emit(patch = {}) {
    this.state = {
      ...this.state,
      ...patch,
      updatedAt: nowLabel()
    };
    this.listeners.forEach((listener) => listener(this.snapshot()));
  }

  useDemo(reason) {
    this.state = {
      ...cloneState(mockState),
      lastError: reason || "BLE native module is not available in this build.",
      updatedAt: nowLabel()
    };
    this.listeners.forEach((listener) => listener(this.snapshot()));
  }

  async start() {
    if (this.device || this.state.connectionState === "scanning" || this.state.connectionState === "connecting") {
      return this.snapshot();
    }

    if (!this.manager) {
      this.manager = loadBleManager();
    }

    if (!this.manager) {
      this.useDemo("BLE requires a native Expo development build with react-native-ble-plx.");
      return this.snapshot();
    }

    const allowed = await requestBluetoothPermissions();
    if (!allowed) {
      this.emit({ connectionState: "permission_denied", lastError: "Bluetooth permission was denied." });
      return this.snapshot();
    }

    const state = await this.manager.state();
    if (state === "PoweredOn") {
      this.scan();
      return this.snapshot();
    }

    this.emit({ connectionState: "waiting_bluetooth", lastError: null });
    const subscription = this.manager.onStateChange((nextState) => {
      if (nextState === "PoweredOn") {
        subscription.remove();
        this.scan();
      }
    }, true);

    return this.snapshot();
  }

  scan() {
    this.cleanupScan();
    this.emit({ connected: false, connectionState: "scanning", lastError: null });

    this.manager.startDeviceScan([BLE_SERVICE_UUID], null, (error, scannedDevice) => {
      if (this.state.connectionState === "connecting") return;

      if (error) {
        this.cleanupScan();
        this.emit({ connectionState: "scan_error", lastError: error.message });
        return;
      }

      if (!scannedDevice) return;
      const name = scannedDevice.name || scannedDevice.localName || "";
      const hasExpectedName = !name || name === BLE_DEVICE_NAME;
      if (!hasExpectedName) return;

      this.connect(scannedDevice);
    });

    this.scanTimeout = setTimeout(() => {
      this.cleanupScan();
      this.emit({ connectionState: "not_found", lastError: `No ${BLE_DEVICE_NAME} device found nearby.` });
    }, 15000);
  }

  cleanupScan() {
    if (this.manager) {
      this.manager.stopDeviceScan();
    }
    if (this.scanTimeout) {
      clearTimeout(this.scanTimeout);
      this.scanTimeout = null;
    }
  }

  async connect(scannedDevice) {
    this.cleanupScan();
    this.emit({ connectionState: "connecting", deviceName: scannedDevice.name || scannedDevice.localName || BLE_DEVICE_NAME });

    try {
      const connectedDevice = await scannedDevice.connect();
      this.device = await connectedDevice.discoverAllServicesAndCharacteristics();
      this.disconnectSubscription = this.device.onDisconnected(() => {
        this.cleanupConnection();
        this.emit({ connected: false, connectionState: "disconnected", lastError: null });
        setTimeout(() => this.start(), 1200);
      });

      this.notificationSubscription = this.device.monitorCharacteristicForService(
        BLE_SERVICE_UUID,
        BLE_CHARACTERISTIC_UUID,
        (error, characteristic) => {
          if (error) {
            this.emit({ lastError: error.message });
            return;
          }
          if (characteristic?.value) {
            this.handleIncomingText(base64ToAscii(characteristic.value));
          }
        }
      );

      this.emit({ connected: true, connectionState: "connected", lastError: null });
      await this.readInitialValue();
      await this.refresh();
    } catch (error) {
      this.cleanupConnection();
      this.emit({ connected: false, connectionState: "connect_error", lastError: error.message });
      setTimeout(() => this.start(), 1800);
    }
  }

  cleanupConnection() {
    if (this.notificationSubscription) {
      this.notificationSubscription.remove();
      this.notificationSubscription = null;
    }
    if (this.disconnectSubscription) {
      this.disconnectSubscription.remove();
      this.disconnectSubscription = null;
    }
    this.device = null;
  }

  async readInitialValue() {
    if (!this.device) return;
    try {
      const characteristic = await this.device.readCharacteristicForService(
        BLE_SERVICE_UUID,
        BLE_CHARACTERISTIC_UUID
      );
      if (characteristic?.value) {
        this.handleIncomingText(base64ToAscii(characteristic.value));
      }
    } catch (error) {
      this.emit({ lastError: error.message });
    }
  }

  async refresh() {
    if (!this.device) {
      return this.start();
    }
    await this.write("info?");
    await this.write("sensors?");
    await this.write("config?");
    await this.write("light?");
    return this.snapshot();
  }

  async write(command) {
    if (!this.device) {
      await this.start();
      return this.snapshot();
    }

    try {
      const line = String(command).trim().slice(0, MAX_COMMAND_LENGTH);
      await this.device.writeCharacteristicWithResponseForService(
        BLE_SERVICE_UUID,
        BLE_CHARACTERISTIC_UUID,
        asciiToBase64(line)
      );
    } catch (error) {
      this.emit({ lastError: error.message || "BLE write failed." });
    }
    return this.snapshot();
  }

  handleIncomingText(text) {
    String(text)
      .split(/\r?\n/)
      .map((line) => line.trim())
      .filter(Boolean)
      .forEach((line) => this.parseLine(line));
  }

  parseLine(line) {
    if (line.startsWith("[ERR]")) {
      this.emit({ lastError: line.replace("[ERR]", "").trim() || line });
      return;
    }

    const match = line.match(/^\[(INF|DAT)]\s+([^:]+):\s*(.*)$/);
    if (!match) return;

    const section = match[2].trim();
    const fields = parseFields(match[3]);

    if (section === "device") {
      this.emit({
        connected: true,
        connectionState: "connected",
        deviceName: fields.name || this.state.deviceName,
        firmware: fields.firmware || this.state.firmware,
        protocol: fields.protocol || this.state.protocol,
        productId: fields.product_id || this.state.productId,
        uptimeS: fields.uptime_s ? Number(fields.uptime_s) : this.state.uptimeS,
        lastError: null
      });
      return;
    }

    if (section === "sensors") {
      this.emit({
        sensors: updateSensorValues(this.state, fields),
        lastError: null
      });
      return;
    }

    if (section === "config") {
      this.emit({
        config: fields,
        lastError: null
      });
      return;
    }

    if (section === "light") {
      this.emit({
        light: {
          ...this.state.light,
          enabled: fields.enabled !== undefined ? parseBoolean(fields.enabled) : this.state.light.enabled,
          mode: fields.mode || this.state.light.mode,
          brightness: fields.brightness !== undefined ? clamp16(fields.brightness) : this.state.light.brightness,
          kelvin: fields.kelvin !== undefined ? clamp16(fields.kelvin) : this.state.light.kelvin,
          coldMin: fields.cold_min ?? this.state.light.coldMin,
          coldMax: fields.cold_max ?? this.state.light.coldMax,
          warmMin: fields.warm_min ?? this.state.light.warmMin,
          warmMax: fields.warm_max ?? this.state.light.warmMax
        },
        lastError: null
      });
    }
  }
}

const client = new DeskBridgeBleClient();

export function subscribeToDeskBridgeState(listener) {
  return client.subscribe(listener);
}

export async function startDeskBridgeConnection() {
  return client.start();
}

export async function fetchDeskBridgeState() {
  return client.refresh();
}

export async function setLightPower(enabled) {
  await client.write(`light enabled ${enabled ? "on" : "off"}`);
  return client.snapshot();
}

export async function setLightValue(field, value) {
  const commandField = field === "kelvin" ? "kelvin" : "brightness";
  await client.write(`light ${commandField} ${clamp16(value)}`);
  return client.snapshot();
}

export async function setLightMode(mode) {
  await client.write(`light mode ${mode}`);
  return client.snapshot();
}
