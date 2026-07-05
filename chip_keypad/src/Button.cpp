#include "Button.h"

#include "pins_config.h"

Button::Button(uint8_t index, uint32_t pin)
    : index_(index),
      pin_(pin),
      oneButton_(pin, KeypadConfig::BUTTON_ACTIVE_LOW,
                 KeypadConfig::BUTTON_PULLUP) {}

void Button::begin(ButtonEventCallback callback) {
  callback_ = callback;

  configureTimings(KeypadConfig::DEBOUNCE_MS, KeypadConfig::CLICK_MS,
                   KeypadConfig::LONG_PRESS_MS);

  oneButton_.attachPress(handlePressed, this);
  oneButton_.attachClick(handleShortReleased, this);
  oneButton_.attachLongPressStop(handleLongReleased, this);
}

void Button::configureTimings(unsigned long debounceMs, unsigned long clickMs,
                              unsigned long longPressMs) {
  oneButton_.setDebounceMs(debounceMs);
  oneButton_.setClickMs(clickMs);
  oneButton_.setPressMs(longPressMs);
}

void Button::tick() {
  oneButton_.tick();
}

bool Button::isPressed() const {
  return digitalRead(pin_) == HIGH;
}

void Button::handlePressed(void* context) {
  static_cast<Button*>(context)->emit(ButtonEventType::Pressed);
}

void Button::handleShortReleased(void* context) {
  static_cast<Button*>(context)->emit(ButtonEventType::ShortReleased);
}

void Button::handleLongReleased(void* context) {
  static_cast<Button*>(context)->emit(ButtonEventType::LongReleased);
}

void Button::emit(ButtonEventType event) {
  if (callback_ != nullptr) {
    callback_(index_, event);
  }
}
