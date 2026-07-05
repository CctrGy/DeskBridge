const $ = (selector) => document.querySelector(selector);
const $$ = (selector) => Array.from(document.querySelectorAll(selector));
const notifications = $("#notifications");
let notificationCount = 0;
let keypadActionOptions = [];
let connectedState = false;

async function api(method, ...args) {
  if (!window.pywebview || !window.pywebview.api) {
    addNotification("WEB", "ERROR", "Python bridge is not ready.");
    return null;
  }
  try {
    return await window.pywebview.api[method](...args);
  } catch (error) {
    addNotification("WEB", "ERROR", String(error));
    return null;
  }
}

function valueOf(selector) {
  const node = $(selector);
  return node ? node.value : "";
}

function setText(selector, value) {
  const node = $(selector);
  if (node) node.textContent = value ?? "-";
}

function setValue(selector, value) {
  const node = $(selector);
  if (node) node.value = value ?? "";
}

function setChecked(selector, value) {
  const node = $(selector);
  if (node) node.checked = value === true;
}

function fillSelect(selector, values) {
  const node = $(selector);
  if (!node) return;
  const current = node.value;
  node.innerHTML = "";
  for (const value of values || []) {
    const option = document.createElement("option");
    option.value = String(value).split(" ")[0];
    option.textContent = value;
    node.append(option);
  }
  if (current) node.value = current;
}

function fillSelectNode(node, values, selectedValue = "") {
  if (!node) return;
  const current = selectedValue || node.value;
  node.innerHTML = "";
  for (const value of values || []) {
    const option = document.createElement("option");
    option.value = String(value).split(" ")[0];
    option.textContent = value;
    node.append(option);
  }
  if (current) node.value = current;
}

function keyActionValues() {
  return $$(".key-action-select").map((node) => node.value || "none");
}

function updateKeyActionControls(actions) {
  const selected = Array.isArray(actions) ? actions : [];
  $$(".key-action-select").forEach((node) => {
    const index = Number(node.dataset.keyIndex || "0");
    fillSelectNode(node, keypadActionOptions, selected[index] || "none");
  });
}

function updateState(state) {
  if (!state) return;
  connectedState = state.connected === true;
  setText("#statusText", state.connected ? "Connected" : "Disconnected");
  setText("#portText", state.port || "Auto");
  setText("#usbIdText", state.usb_id || "1209:DB01");
  setText("#firmwareText", state.firmware || "-");
  setText("#protocolText", state.protocol || "-");
  setText("#rtcText", state.rtc || "--:--:--");
  setText("#profileText", state.profile || "Default");
  $("#oledLayout").value = state.oled_layout || "Home monitor";
  $("#temperatureUnit").value = state.temperature_unit || "Celsius (C)";
  $("#sensorSamples").value = state.sensor_samples || "3";
  $("#readInterval").value = state.read_interval || "1000ms";
  setChecked("#backendNotifyTemperature", state.backend_notify_temperature);
  setChecked("#backendStartupEnabled", state.backend_startup_enabled);
  setChecked("#backendNotifyHumidity", state.backend_notify_humidity);
  setChecked("#backendNotifyCo2", state.backend_notify_co2);
  setChecked("#backendNotifyLux", state.backend_notify_lux);
  updateKeyActionControls(state.key_actions);
  updateConnectionControls();
}

function updateConnectionControls() {
  const toggle = $("#connectionToggleBtn");
  if (toggle) {
    toggle.textContent = connectedState ? "Disconnect" : "Connect";
    toggle.classList.toggle("primary", !connectedState);
    toggle.classList.toggle("danger", connectedState);
  }
  setText("#titleStatusText", connectedState ? "Connected" : "Disconnected");
  $("#titleStatusText")?.classList.toggle("connected", connectedState);
  $(".status-dot")?.classList.toggle("connected", connectedState);
  setText("#titleDeviceText", connectedState ? "DeskBridge Core" : "No peripheral");
  $("#settingsConnectBtn")?.classList.toggle("state-active", !connectedState);
  $("#settingsDisconnectBtn")?.classList.toggle("state-active", connectedState);
}

function addNotification(source, kind, message) {
  notificationCount += 1;
  $("#notificationCount").textContent = String(notificationCount);
  const row = document.createElement("div");
  const normalizedKind = String(kind || "INFO").toLowerCase();
  row.className = "notification";
  row.innerHTML = `
    <span>${new Date().toLocaleTimeString()}</span>
    <span>${escapeHtml(source || "GUI")}</span>
    <strong class="${normalizedKind}">${escapeHtml(kind || "INFO")}</strong>
    <span>${escapeHtml(message || "")}</span>
  `;
  notifications.prepend(row);
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}

function handleResult(result) {
  if (!result) return;
  if (result.notification) addNotification(result.notification.source, result.notification.kind, result.notification.message);
  if (result.state) updateState(result.state);
  if (result.error) addNotification("GUI", "ERROR", result.error);
}

async function refreshState() {
  updateState(await api("get_state"));
}

async function startupProbe() {
  handleResult(await api("startup_probe"));
}

async function loadPorts() {
  fillSelect("#portSelect", await api("list_ports") || ["Auto"]);
}

async function loadOptions() {
  const options = await api("get_options");
  if (!options) return;
  keypadActionOptions = options.keypad_actions || [];
  fillSelect("#keypadAction", options.keypad_actions);
  updateKeyActionControls(keyActionValues());
  fillSelect("#lightField", options.light_fields);
  fillSelect("#resetTarget", options.reset_targets);
  fillSelect("#sensorSelector", options.sensors);
}

async function connect() {
  handleResult(await api("connect", valueOf("#portSelect") || null));
}

async function disconnect() {
  handleResult(await api("disconnect"));
}

async function toggleConnection() {
  if (connectedState) {
    await disconnect();
  } else {
    await connect();
  }
}

function commandValues(button) {
  const values = {};
  if (button.dataset.value) values.value = valueOf(button.dataset.value);
  if (button.dataset.selector) values.selector = valueOf(button.dataset.selector);
  if (button.dataset.button) values.button = valueOf(button.dataset.button);
  if (button.dataset.actionValue) values.action = valueOf(button.dataset.actionValue);
  if (button.dataset.field) values.field = valueOf(button.dataset.field);
  if (button.dataset.target) values.target = valueOf(button.dataset.target);
  return values;
}

async function executeControl(button) {
  handleResult(await api("execute_control", button.dataset.command, commandValues(button)));
}

async function savePreferences() {
  handleResult(await api("save_preferences", {
    oled_layout: valueOf("#oledLayout"),
    temperature_unit: valueOf("#temperatureUnit"),
    sensor_samples: valueOf("#sensorSamples"),
    read_interval: valueOf("#readInterval"),
    wireless_enabled: valueOf("#wirelessEnabledValue") === "on",
    backend_notify_temperature: $("#backendNotifyTemperature")?.checked === true,
    backend_notify_humidity: $("#backendNotifyHumidity")?.checked === true,
    backend_notify_co2: $("#backendNotifyCo2")?.checked === true,
    backend_notify_lux: $("#backendNotifyLux")?.checked === true,
    key_actions: keyActionValues()
  }));
}

async function saveBackendStartup() {
  handleResult(await api("set_backend_startup", $("#backendStartupEnabled")?.checked === true));
}

async function applyKeyActions() {
  const actions = keyActionValues();
  handleResult(await api("execute_control", "keypad.actions.apply", { actions }));
}

function showPanel(panelId) {
  $$(".view-panel").forEach((panel) => panel.classList.toggle("active", panel.id === panelId));
  $$(".nav-item").forEach((button) => button.classList.toggle("active", button.dataset.panel === panelId));
}

window.addEventListener("pywebviewready", async () => {
  await Promise.all([loadPorts(), loadOptions()]);
  await startupProbe();
  addNotification("GUI", "INFO", "Web interface ready.");
});

$("#connectionToggleBtn").addEventListener("click", toggleConnection);
$("#settingsConnectBtn").addEventListener("click", connect);
$("#settingsDisconnectBtn").addEventListener("click", disconnect);
$("#minimizeWindowBtn").addEventListener("click", () => api("window_minimize"));
$("#closeWindowBtn").addEventListener("click", () => api("window_close"));
$("#rescanPortsBtn").addEventListener("click", loadPorts);
$("#settingsBtn").addEventListener("click", async () => {
  await loadPorts();
  $("#settingsDialog").showModal();
});
$("#savePrefsBtn").addEventListener("click", savePreferences);
$("#saveKeyPrefsBtn").addEventListener("click", savePreferences);
$("#saveBackendAlertsBtn").addEventListener("click", savePreferences);
$("#saveBackendStartupBtn").addEventListener("click", saveBackendStartup);
$("#applyKeyActionsBtn").addEventListener("click", applyKeyActions);
$("#clearNotificationsBtn").addEventListener("click", () => {
  notifications.innerHTML = "";
  notificationCount = 0;
  $("#notificationCount").textContent = "0";
});

$$(".nav-item").forEach((button) => {
  button.addEventListener("click", () => showPanel(button.dataset.panel));
});

$$("[data-command]").forEach((button) => {
  button.addEventListener("click", () => executeControl(button));
});

let draggingWindow = false;
let dragOffset = { x: 0, y: 0 };
const titlebar = $("#dragTitlebar");

titlebar.addEventListener("mousedown", (event) => {
  if (event.target.closest("button")) return;
  draggingWindow = true;
  dragOffset = {
    x: event.screenX - window.screenX,
    y: event.screenY - window.screenY
  };
});

window.addEventListener("mouseup", () => {
  draggingWindow = false;
});

window.addEventListener("mousemove", (event) => {
  if (!draggingWindow) return;
  api("window_drag", event.screenX, event.screenY, dragOffset.x, dragOffset.y);
});
