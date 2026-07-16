#include "Button.h"

Button::Button(
    uint8_t pin,
    uint32_t debounceTime,
    uint32_t longPressTime)
{
    _pin = pin;

    _debounceTime = debounceTime;
    _longPressTime = longPressTime;

    _lastReading = HIGH;

    _lastDebounceTime = 0;
    _pressedTime = 0;

    _state = State::Released;

    _event = Event::None;
}

void Button::begin()
{
    pinMode(_pin, INPUT_PULLUP);
}

void Button::update()
{
    bool reading = digitalRead(_pin);

    if (reading != _lastReading)
    {
        _lastReading = reading;
        _lastDebounceTime = millis();
    }
    /*
    since real switch don't change cleanly, this check is to ignore rapid changes 
    until the signal has stayed stable for at least set debounceTime = 40ms
    */ 
    if ((millis() - _lastDebounceTime) < _debounceTime)
        return;

    switch (_state)
    {
        case State::Released:

            if (reading == LOW)
            {
                _state = State::Pressed;
                _pressedTime = millis();
            }

            break;

        case State::Pressed:

            if (reading == HIGH)
            {
                uint32_t duration = millis() - _pressedTime;

                if (duration >= _longPressTime)
                    _event = Event::LongPress;
                else
                    _event = Event::ShortPress;

                _state = State::Released;
            }

            break;
    }
}

Button::Event Button::getEvent()
{
    Event e = _event;
    _event = Event::None;
    return e;
}