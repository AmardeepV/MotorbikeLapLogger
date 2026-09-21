
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

    void sendStatus(const String& status);

    void onConnected();
    void onDisconnected();

    bool isConnected() const;

private:
    Command _command{Command::None};
    bool _connected{false};
};