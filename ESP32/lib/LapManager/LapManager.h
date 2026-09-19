#pragma once
#include <Arduino.h>
class LapManager
{
    public:
        enum class Command
        {
            None,
            Lap,
            Stop
        };

        enum class Event
        {
            None,
            LapStarted,
            LapFinished,
            LoggingStopped
        };

        LapManager();
        
        //void update(Button::Event event, uint32_t now);
        void update(Command command, uint32_t now);
        Event getEvent();
        bool isLogging() const;

        uint32_t getCurrentLap() const;
        uint32_t getLastLapTime() const;
        uint32_t getCurrentLapRunningTime(uint32_t now) const;

    private:
        enum class LapState
        {
            Idle,
            Running
        };

        uint32_t _currentLapNumber;
        uint32_t _lapStartTime;
        uint32_t _lastLapTime;

        LapState _state;
        Event _event;

        void startLap(uint32_t now);
        void finishLap(uint32_t now);

};