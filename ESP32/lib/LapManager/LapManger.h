#pragma once

#include <Arduino.h>
#include "Button.h"

class LapManager
{
public:

    enum class State
    {
        Ready,
        Logging
    };

    enum class Event
    {
        None,
        LoggingStarted,
        NewLap,
        LoggingStopped
    };

    LapManager();

    void begin();

    void update(Button::Event buttonEvent, uint32_t now);

    bool isLogging() const;

    uint16_t getCurrentLap() const;

    uint32_t getCurrentLapTime(uint32_t now) const;

    uint32_t getLastLapTime() const;

    uint32_t getTotalTime(uint32_t now) const;

    State getState() const;

    Event getEvent();

private:

    State _state;

    Event _event;

    uint16_t _currentLap;

    uint32_t _rideStartTime;

    uint32_t _lapStartTime;

    uint32_t _lastLapTime;
};