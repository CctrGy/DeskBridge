#include <Arduino.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <KeypadProtocol.h>
#include "Button.h"
#include "pins_config.h"

namespace Protocol = DeskBridgeKeypadProtocol;

namespace {

HardwareSerial CoreSerial(KeypadConfig::UART_RX_PIN, KeypadConfig::UART_TX_PIN);

Button buttons[KeypadConfig::BUTTON_COUNT] = {
  Button(0, KeypadConfig::BUTTON_PINS[0]),
  Button(1, KeypadConfig::BUTTON_PINS[1]),
  Button(2, KeypadConfig::BUTTON_PINS[2]),
  Button(3, KeypadConfig::BUTTON_PINS[3]),
  Button(4, KeypadConfig::BUTTON_PINS[4]),
};

enum EventFlag : uint16_t {
  EventCoreCommand = 1 << 6,
  EventButtonEdge = 1 << 9,
};

enum ButtonEventMask : uint8_t {
  ButtonEventPressed = 1 << 0,
  ButtonEventShortReleased = 1 << 1,
  ButtonEventLongReleased = 1 << 2,
  ButtonEventAll = ButtonEventPressed | ButtonEventShortReleased |
                   ButtonEventLongReleased,
};

struct RuntimeState {
  uint16_t eventFlags = 0;
  uint8_t buttonPressedMask = 0;
  uint8_t buttonReleasedMask = 0;
  uint8_t buttonDownMask = 0;
  uint8_t lastButton = 0xFF;
  uint8_t lastButtonEdge = 0;
  uint8_t lastButtonEventCode = 0;
  uint8_t lastActionId = 0;
  char lastActionName[16] = {};
  uint8_t buttonAssignments[KeypadConfig::BUTTON_COUNT] = {};
  uint8_t enabledButtonEvents[KeypadConfig::BUTTON_COUNT] = {
      ButtonEventAll, ButtonEventAll, ButtonEventAll, ButtonEventAll,
      ButtonEventAll};

  bool stripEnabled = false;
  uint8_t stripMode = 0;
  uint16_t brightness = 0;
  uint16_t kelvin = 0;
  uint16_t coldMin = 0;
  uint16_t coldMax = 65535;
  uint16_t warmMin = 0;
  uint16_t warmMax = 65535;
  uint16_t brightnessStep = 0;
  uint16_t kelvinStep = 0;
  uint16_t clickMs = KeypadConfig::CLICK_MS;
  uint16_t longMs = KeypadConfig::LONG_PRESS_MS;
  uint16_t repeatMs = KeypadConfig::TICK_INTERVAL_MS;
};

RuntimeState state;
unsigned long lastTickMs = 0;
char rxLine[KeypadConfig::UART_LINE_CAPACITY] = {};
uint8_t rxLength = 0;

bool equalsIgnoreCase(const char* left, const char* right) {
  while (*left != '\0' && *right != '\0') {
    if (tolower(*left) != tolower(*right)) {
      return false;
    }
    ++left;
    ++right;
  }

  return *left == '\0' && *right == '\0';
}

uint8_t eventMaskForType(ButtonEventType type) {
  switch (type) {
    case ButtonEventType::Pressed:
      return ButtonEventPressed;
    case ButtonEventType::ShortReleased:
      return ButtonEventShortReleased;
    case ButtonEventType::LongReleased:
      return ButtonEventLongReleased;
  }

  return 0;
}

uint8_t eventCodeForType(ButtonEventType type) {
  switch (type) {
    case ButtonEventType::Pressed:
      return 1;
    case ButtonEventType::ShortReleased:
      return 2;
    case ButtonEventType::LongReleased:
      return 3;
  }

  return 0;
}

const char* actionName(uint8_t actionId) {
  switch (actionId) {
    case 1:
      return "MUTE";
    case 2:
      return "VOL_UP";
    case 3:
      return "VOL_DOWN";
    case 4:
      return "PLAY";
    case 5:
      return "NEXT";
    case 6:
      return "PREV";
    case 7:
      return "F13";
    case 8:
      return "F14";
    case 9:
      return "F15";
    default:
      return "NONE";
  }
}

bool eventEnabled(uint8_t button, ButtonEventType type) {
  return (state.enabledButtonEvents[button] & eventMaskForType(type)) != 0;
}

void setEventPending() {
  digitalWrite(KeypadConfig::EVENT_PIN, HIGH);
}

void clearEventPending() {
  state.eventFlags = 0;
  state.buttonPressedMask = 0;
  state.buttonReleasedMask = 0;
  state.lastButton = 0xFF;
  state.lastButtonEdge = 0;
  state.lastButtonEventCode = 0;
  state.lastActionId = 0;
  state.lastActionName[0] = '\0';
  digitalWrite(KeypadConfig::EVENT_PIN, LOW);
}

void handleButtonEvent(uint8_t button, ButtonEventType type) {
  if (button >= KeypadConfig::BUTTON_COUNT) {
    return;
  }

  if (!eventEnabled(button, type)) {
    return;
  }

  const uint8_t mask = 1 << button;
  state.eventFlags |= EventButtonEdge;
  state.lastButton = button;
  state.lastButtonEventCode = eventCodeForType(type);
  state.lastActionId = state.buttonAssignments[button];
  strncpy(state.lastActionName, actionName(state.lastActionId),
          sizeof(state.lastActionName) - 1);
  state.lastActionName[sizeof(state.lastActionName) - 1] = '\0';

  if (type == ButtonEventType::Pressed) {
    state.buttonPressedMask |= mask;
    state.buttonDownMask |= mask;
    state.lastButtonEdge = 1;
  } else {
    state.buttonReleasedMask |= mask;
    state.buttonDownMask &= ~mask;
    state.lastButtonEdge = 2;
  }

  setEventPending();
}

void updateButtons() {
  unsigned long now = millis();
  if (now - lastTickMs < KeypadConfig::TICK_INTERVAL_MS) {
    return;
  }

  lastTickMs = now;
  for (Button& button : buttons) {
    button.tick();
  }
}

void updateStatePin() {
  bool anyPressed = false;
  uint8_t downMask = 0;

  for (const Button& button : buttons) {
    const uint8_t index = static_cast<uint8_t>(&button - buttons);
    if (button.isPressed()) {
      anyPressed = true;
      downMask |= 1 << index;
    }
  }

  state.buttonDownMask = downMask;
  digitalWrite(KeypadConfig::STATE_PIN, anyPressed ? HIGH : LOW);
}

void applyButtonTimings() {
  for (Button& button : buttons) {
    button.configureTimings(KeypadConfig::DEBOUNCE_MS, state.clickMs,
                            state.longMs);
  }
}

uint16_t valueAfterPrefix(const char* line, const char* prefix) {
  return static_cast<uint16_t>(strtoul(line + strlen(prefix), nullptr, 10));
}

void replyStrip() {
  CoreSerial.print("OK EN=");
  CoreSerial.print(state.stripEnabled ? 1 : 0);
  CoreSerial.print(" MODE=");
  CoreSerial.print(state.stripMode);
  CoreSerial.print(" VAL=");
  CoreSerial.print(state.brightness);
  CoreSerial.print(" KEL=");
  CoreSerial.print(state.kelvin);
  CoreSerial.print(" CMIN=");
  CoreSerial.print(state.coldMin);
  CoreSerial.print(" CMAX=");
  CoreSerial.print(state.coldMax);
  CoreSerial.print(" WMIN=");
  CoreSerial.print(state.warmMin);
  CoreSerial.print(" WMAX=");
  CoreSerial.print(state.warmMax);
  CoreSerial.print(" BSTEP=");
  CoreSerial.print(state.brightnessStep);
  CoreSerial.print(" KSTEP=");
  CoreSerial.print(state.kelvinStep);
  CoreSerial.print(" CT=");
  CoreSerial.print(state.clickMs);
  CoreSerial.print(" LT=");
  CoreSerial.print(state.longMs);
  CoreSerial.print(" RT=");
  CoreSerial.print(state.repeatMs);
  CoreSerial.print(" PWM_C=0 PWM_W=0\n");
}

void replyPower() {
  CoreSerial.println("OK MA=0 RAW=0 MV=0 VRAW=0 MW=0");
}

void replyInfo() {
  CoreSerial.print("OK DEV=");
  CoreSerial.print(Protocol::DeviceId);
  CoreSerial.print(" PROTO=");
  CoreSerial.print(Protocol::ProtocolText);
  CoreSerial.print(" ");
  CoreSerial.print(Protocol::Field::ProtocolVersion);
  CoreSerial.print("0x");
  if (Protocol::ProtocolVersion < 0x10) {
    CoreSerial.print('0');
  }
  CoreSerial.print(Protocol::ProtocolVersion, HEX);
  CoreSerial.print(" NAME=");
  CoreSerial.print(Protocol::DeviceName);
  CoreSerial.print(" FW=");
  CoreSerial.println(Protocol::FirmwareVersion);
}

void replyEvents(bool includeEventFlags) {
  CoreSerial.print("OK");
  if (includeEventFlags) {
    CoreSerial.print(" EVT=");
    CoreSerial.print(state.eventFlags);
  }
  CoreSerial.print(" BP=");
  CoreSerial.print(state.buttonPressedMask);
  CoreSerial.print(" BR=");
  CoreSerial.print(state.buttonReleasedMask);
  CoreSerial.print(" BD=");
  CoreSerial.print(state.buttonDownMask);
  CoreSerial.print(" LB=");
  CoreSerial.print(state.lastButton);
  CoreSerial.print(" BE=");
  CoreSerial.print(state.lastButtonEdge);
  CoreSerial.print(" EC=");
  CoreSerial.print(state.lastButtonEventCode);
  CoreSerial.print(" AID=");
  CoreSerial.print(state.lastActionId);
  CoreSerial.print(" ACT=");
  CoreSerial.println(state.lastActionName[0] != '\0' ? state.lastActionName : "NONE");
  clearEventPending();
}

void replyButtonAssignments() {
  CoreSerial.print("OK");
  for (uint8_t i = 0; i < KeypadConfig::BUTTON_COUNT; ++i) {
    CoreSerial.print(" B");
    CoreSerial.print(i);
    CoreSerial.print("=");
    CoreSerial.print(state.buttonAssignments[i]);
  }
  CoreSerial.print('\n');
}

void replyEventConfig() {
  CoreSerial.print("OK");
  for (uint8_t i = 0; i < KeypadConfig::BUTTON_COUNT; ++i) {
    CoreSerial.print(" B");
    CoreSerial.print(i);
    CoreSerial.print("=");
    CoreSerial.print(state.enabledButtonEvents[i]);
  }
  CoreSerial.print('\n');
}

void resetDefaults() {
  state = RuntimeState{};
  clearEventPending();
  applyButtonTimings();
}

void handleButtonAssignmentCommand(const char* line) {
  const char* args = line + strlen(Protocol::Prefix::SetButtonAction);
  const uint8_t button = static_cast<uint8_t>(strtoul(args, nullptr, 10));
  const char* comma = strchr(args, ',');
  if (button >= KeypadConfig::BUTTON_COUNT || comma == nullptr) {
    CoreSerial.println("ERR ARG");
    return;
  }

  state.buttonAssignments[button] =
      static_cast<uint8_t>(strtoul(comma + 1, nullptr, 10));
  CoreSerial.println("OK");
}

bool parseButtonToken(const char* token, uint8_t& button) {
  if ((token[0] != 'B' && token[0] != 'b') || token[1] == '\0') {
    return false;
  }

  button = static_cast<uint8_t>(strtoul(token + 1, nullptr, 10));
  return button < KeypadConfig::BUTTON_COUNT;
}

bool parseEventToken(const char* token, uint8_t& mask) {
  if (equalsIgnoreCase(token, "PRESSED")) {
    mask = ButtonEventPressed;
    return true;
  }
  if (equalsIgnoreCase(token, "SHORT_RELEASED")) {
    mask = ButtonEventShortReleased;
    return true;
  }
  if (equalsIgnoreCase(token, "LONG_RELEASED")) {
    mask = ButtonEventLongReleased;
    return true;
  }
  if (equalsIgnoreCase(token, "ALL")) {
    mask = ButtonEventAll;
    return true;
  }

  return false;
}

void handleEventConfigCommand(const char* line) {
  char command[8] = {};
  char buttonToken[4] = {};
  char eventToken[16] = {};
  char stateToken[12] = {};

  if (sscanf(line, "%7s %3s %15s %11s", command, buttonToken, eventToken,
             stateToken) != 4) {
    CoreSerial.println("ERR ARG");
    return;
  }

  uint8_t button = 0;
  uint8_t mask = 0;
  if (!parseButtonToken(buttonToken, button) ||
      !parseEventToken(eventToken, mask)) {
    CoreSerial.println("ERR ARG");
    return;
  }

  if (equalsIgnoreCase(stateToken, "enabled") ||
      equalsIgnoreCase(stateToken, "enable") ||
      equalsIgnoreCase(stateToken, "1")) {
    state.enabledButtonEvents[button] |= mask;
    CoreSerial.println("OK");
    return;
  }

  if (equalsIgnoreCase(stateToken, "disabled") ||
      equalsIgnoreCase(stateToken, "disable") ||
      equalsIgnoreCase(stateToken, "0")) {
    state.enabledButtonEvents[button] &= ~mask;
    CoreSerial.println("OK");
    return;
  }

  CoreSerial.println("ERR ARG");
}

void handleCommand(const char* line) {
  if (strcmp(line, Protocol::Command::Ping) == 0) {
    CoreSerial.println(Protocol::Response::Pong);
  } else if (strcmp(line, Protocol::Command::Info) == 0) {
    replyInfo();
  } else if (strcmp(line, Protocol::Command::Strip) == 0) {
    replyStrip();
  } else if (strcmp(line, Protocol::Command::Power) == 0) {
    replyPower();
  } else if (strcmp(line, Protocol::Command::Events) == 0) {
    replyEvents(true);
  } else if (strcmp(line, Protocol::Command::Buttons) == 0) {
    replyEvents(false);
} else if (strcmp(line, Protocol::Command::Action) == 0) {
    CoreSerial.print("OK AID=");
    CoreSerial.print(state.lastActionId);
    CoreSerial.print(" ACT=");
    CoreSerial.println(state.lastActionName[0] != '\0' ? state.lastActionName : "NONE");
  } else if (strcmp(line, Protocol::Command::ButtonActionList) == 0) {
    CoreSerial.println("OK N=9 A0=NONE A1=MUTE A2=VOL_UP A3=VOL_DOWN A4=PLAY A5=NEXT A6=PREV A7=F13 A8=F14 A9=F15");
  } else if (strcmp(line, Protocol::Command::ButtonAssignments) == 0) {
    replyButtonAssignments();
  } else if (strcmp(line, "EVENT?") == 0) {
    replyEventConfig();
  } else if (strcmp(line, Protocol::Command::ClearEvents) == 0) {
    clearEventPending();
    CoreSerial.println("OK");
  } else if (strcmp(line, Protocol::Command::ResetDefaults) == 0) {
    resetDefaults();
    CoreSerial.println("OK");
  } else if (Protocol::startsWith(line, Protocol::Prefix::SetEnabled)) {
    state.stripEnabled = valueAfterPrefix(line, Protocol::Prefix::SetEnabled) != 0;
    CoreSerial.println("OK");
  } else if (Protocol::startsWith(line, Protocol::Prefix::SetMode)) {
    state.stripMode = static_cast<uint8_t>(valueAfterPrefix(line, Protocol::Prefix::SetMode));
    CoreSerial.println("OK");
  } else if (Protocol::startsWith(line, Protocol::Prefix::SetBrightness)) {
    state.brightness = valueAfterPrefix(line, Protocol::Prefix::SetBrightness);
    CoreSerial.println("OK");
  } else if (Protocol::startsWith(line, Protocol::Prefix::SetKelvin)) {
    state.kelvin = valueAfterPrefix(line, Protocol::Prefix::SetKelvin);
    CoreSerial.println("OK");
  } else if (Protocol::startsWith(line, Protocol::Prefix::SetColdMin)) {
    state.coldMin = valueAfterPrefix(line, Protocol::Prefix::SetColdMin);
    CoreSerial.println("OK");
  } else if (Protocol::startsWith(line, Protocol::Prefix::SetColdMax)) {
    state.coldMax = valueAfterPrefix(line, Protocol::Prefix::SetColdMax);
    CoreSerial.println("OK");
  } else if (Protocol::startsWith(line, Protocol::Prefix::SetWarmMin)) {
    state.warmMin = valueAfterPrefix(line, Protocol::Prefix::SetWarmMin);
    CoreSerial.println("OK");
  } else if (Protocol::startsWith(line, Protocol::Prefix::SetWarmMax)) {
    state.warmMax = valueAfterPrefix(line, Protocol::Prefix::SetWarmMax);
    CoreSerial.println("OK");
  } else if (Protocol::startsWith(line, Protocol::Prefix::SetBrightnessStep)) {
    state.brightnessStep = valueAfterPrefix(line, Protocol::Prefix::SetBrightnessStep);
    CoreSerial.println("OK");
  } else if (Protocol::startsWith(line, Protocol::Prefix::SetKelvinStep)) {
    state.kelvinStep = valueAfterPrefix(line, Protocol::Prefix::SetKelvinStep);
    CoreSerial.println("OK");
  } else if (Protocol::startsWith(line, Protocol::Prefix::SetClickMs)) {
    state.clickMs = valueAfterPrefix(line, Protocol::Prefix::SetClickMs);
    applyButtonTimings();
    CoreSerial.println("OK");
  } else if (Protocol::startsWith(line, Protocol::Prefix::SetLongMs)) {
    state.longMs = valueAfterPrefix(line, Protocol::Prefix::SetLongMs);
    applyButtonTimings();
    CoreSerial.println("OK");
  } else if (Protocol::startsWith(line, Protocol::Prefix::SetRepeatMs)) {
    state.repeatMs = valueAfterPrefix(line, Protocol::Prefix::SetRepeatMs);
    CoreSerial.println("OK");
  } else if (Protocol::startsWith(line, Protocol::Prefix::SetButtonAction)) {
    handleButtonAssignmentCommand(line);
  } else if (Protocol::startsWith(line, "EVENT ")) {
    handleEventConfigCommand(line);
  } else {
    CoreSerial.println("ERR CMD");
  }
}

void updateUart() {
  while (CoreSerial.available() > 0) {
    const char value = static_cast<char>(CoreSerial.read());
    if (value == '\r') {
      continue;
    }

    if (value == '\n') {
      rxLine[rxLength] = '\0';
      if (rxLength > 0) {
        handleCommand(rxLine);
      }
      rxLength = 0;
      continue;
    }

    if (rxLength + 1 < sizeof(rxLine)) {
      rxLine[rxLength++] = value;
    } else {
      rxLength = 0;
      CoreSerial.println("ERR LONG");
    }
  }
}

}  // namespace

void setup() {
  pinMode(KeypadConfig::EVENT_PIN, OUTPUT);
  pinMode(KeypadConfig::STATE_PIN, OUTPUT);
  digitalWrite(KeypadConfig::EVENT_PIN, LOW);
  digitalWrite(KeypadConfig::STATE_PIN, LOW);

  CoreSerial.begin(KeypadConfig::CORE_UART_BAUD);

  for (Button& button : buttons) {
    button.begin(handleButtonEvent);
  }
}

void loop() {
  updateButtons();
  updateStatePin();
  updateUart();
}
