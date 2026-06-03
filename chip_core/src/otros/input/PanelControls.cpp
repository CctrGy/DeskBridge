#include "input/PanelControls.h"

#include <Arduino.h>
#include <RotaryEncoder.h>

#include "config/DeskConfig.h"
#include "config/pins_config.h"

namespace
{
    constexpr uint32_t buttonDebounceUs = ENCODER_BUTTON_DEBOUNCE_US_DEFAULT;

    RotaryEncoder encoder(ENC_DT_PIN, ENC_CLK_PIN, RotaryEncoder::LatchMode::TWO03);

    long lastEncoderPosition = 0;
    int16_t pendingEncoderDelta = 0;
    volatile uint8_t pendingEncoderClicks = 0;
    volatile uint8_t pendingEscapeClicks = 0;
    volatile bool encoderButtonDown = false;
    volatile bool escapeButtonDown = false;
    volatile uint32_t lastEncoderButtonEdgeUs = 0;
    volatile uint32_t lastEscapeButtonEdgeUs = 0;

    void tickEncoder()
    {
        encoder.tick();
    }

    void handleEncoderButtonChange()
    {
        const uint32_t nowUs = micros();
        if (nowUs - lastEncoderButtonEdgeUs < buttonDebounceUs)
        {
            return;
        }

        lastEncoderButtonEdgeUs = nowUs;
        const bool down = digitalRead(ENC_BTN_PIN) == LOW;
        if (down)
        {
            encoderButtonDown = true;
        }
        else if (encoderButtonDown)
        {
            encoderButtonDown = false;
            if (pendingEncoderClicks < 255)
            {
                ++pendingEncoderClicks;
            }
        }
    }

    void handleEscapeButtonChange()
    {
        const uint32_t nowUs = micros();
        if (nowUs - lastEscapeButtonEdgeUs < buttonDebounceUs)
        {
            return;
        }

        lastEscapeButtonEdgeUs = nowUs;
        const bool down = digitalRead(BTN_ESC_PIN) == LOW;
        if (down)
        {
            escapeButtonDown = true;
        }
        else if (escapeButtonDown)
        {
            escapeButtonDown = false;
            if (pendingEscapeClicks < 255)
            {
                ++pendingEscapeClicks;
            }
        }
    }
}

namespace PanelControls
{
    void begin()
    {
        pinMode(ENC_BTN_PIN, INPUT_PULLUP);
        pinMode(BTN_ESC_PIN, INPUT_PULLUP);
        encoderButtonDown = digitalRead(ENC_BTN_PIN) == LOW;
        escapeButtonDown = digitalRead(BTN_ESC_PIN) == LOW;

        attachInterrupt(digitalPinToInterrupt(ENC_CLK_PIN), tickEncoder, CHANGE);
        attachInterrupt(digitalPinToInterrupt(ENC_DT_PIN), tickEncoder, CHANGE);
        attachInterrupt(digitalPinToInterrupt(ENC_BTN_PIN), handleEncoderButtonChange, CHANGE);
        attachInterrupt(digitalPinToInterrupt(BTN_ESC_PIN), handleEscapeButtonChange, CHANGE);
    }

    void update()
    {
        encoder.tick();

        noInterrupts();
        const long rawPosition = encoder.getPosition();
        interrupts();

        const long position = rawPosition / ENCODER_POSITION_DIVIDER_DEFAULT;
        if (position != lastEncoderPosition)
        {
            pendingEncoderDelta += static_cast<int16_t>(position - lastEncoderPosition);
            lastEncoderPosition = position;
        }
    }

    int16_t consumeEncoderDelta()
    {
        const int16_t delta = pendingEncoderDelta;
        pendingEncoderDelta = 0;
        return delta;
    }

    Event consumeEvent()
    {
        noInterrupts();
        if (pendingEncoderClicks > 0)
        {
            --pendingEncoderClicks;
            interrupts();
            return Event::EncoderClick;
        }

        if (pendingEscapeClicks > 0)
        {
            --pendingEscapeClicks;
            interrupts();
            return Event::EscapeClick;
        }
        interrupts();

        return Event::None;
    }
}
