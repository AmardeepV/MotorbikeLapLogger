#include "LapManager.h"


LapManager::LapManager()
    : _currentLapNumber(0),
      _lapStartTime(0),
      _lastLapTime(0),
      _state(LapState::Idle),
      _event(Event::None)
{
} 

void LapManager::update(Command command, uint32_t now)
{
    switch(_state)
    {
        case LapState::Idle:
            if(command == Command::Lap)
            {
                startLap(now);
                _event = Event::LapStarted;
            }
            break;

        case LapState::Running:
            if(command == Command::Lap)
            {
                finishLap(now);
                _event = Event::LapFinished;
                startLap(now);
            }
            else if(command == Command::Stop)
            {
                finishLap(now);
                _event = Event::LoggingStopped;
                _currentLapNumber = 0;
                _lapStartTime = 0;

            }
            break;
            
    }
}

bool LapManager::isLogging() const
{
    return _state == LapState::Running;
}

uint32_t LapManager::getCurrentLap() const
{
    return _currentLapNumber;
}

uint32_t LapManager::getLastLapTime() const
{
    return _lastLapTime;
}

uint32_t LapManager::getCurrentLapRunningTime(uint32_t now) const
{
    uint32_t currentLapRunningTime = now - _lapStartTime;
    return currentLapRunningTime;
}

void LapManager::startLap(uint32_t now)
{
        _lapStartTime = now;
        _currentLapNumber += 1;
        _state = LapState::Running;
}

void LapManager::finishLap(uint32_t now)
{
    _lastLapTime = now - _lapStartTime;
    _state = LapState::Idle;
   
}

LapManager::Event LapManager::getEvent()
{
    Event e = _event;
    _event = Event::None;
    return e;
}