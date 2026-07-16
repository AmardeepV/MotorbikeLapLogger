#pragma once

#include <Arduino.h>

class Button
{
public:

    enum class Event
    {
        None,
        ShortPress,
        LongPress
    };

    Button(
        uint8_t pin,
        uint32_t debounceTime = 40,
        uint32_t longPressTime = 3000);

    void begin();

    void update();

    Event getEvent();

private:

    enum class State
    {
        Released,
        Pressed
    };

    uint8_t _pin;

    uint32_t _debounceTime;
    uint32_t _longPressTime;

    uint32_t _lastDebounceTime;
    uint32_t _pressedTime;

    bool _lastReading;

    State _state;

    Event _event;
};