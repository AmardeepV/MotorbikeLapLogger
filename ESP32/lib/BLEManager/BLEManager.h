#pragma once

#include <Arduino.h>

class BLEManager
{
public:

    enum class Command
    {
        None,
        Lap,
        Stop,
        Calibrate
    };

    bool begin();

    Command getCommand();
    void setCommand(Command command);

private:

    Command _command{Command::None};
};