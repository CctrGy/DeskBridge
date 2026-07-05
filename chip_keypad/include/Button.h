#pragma once

#include <Arduino.h>
#include <OneButton.h>

enum class ButtonEventType : uint8_t {
  Pressed,
  ShortReleased,
  LongReleased,
};

using ButtonEventCallback = void (*)(uint8_t index, ButtonEventType event);

class Button {
 public:
  Button(uint8_t index, uint32_t pin);

  void begin(ButtonEventCallback callback);
  void configureTimings(unsigned long debounceMs, unsigned long clickMs,
                        unsigned long longPressMs);
  void tick();
  bool isPressed() const;

 private:
  static void handlePressed(void* context);
  static void handleShortReleased(void* context);
  static void handleLongReleased(void* context);

  void emit(ButtonEventType event);

  uint8_t index_;
  uint32_t pin_;
  OneButton oneButton_;
  ButtonEventCallback callback_ = nullptr;
};
