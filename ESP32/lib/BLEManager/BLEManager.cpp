#include "BLEManager.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

static constexpr char SERVICE_UUID[] =
    "6e400001-b5a3-f393-e0a9-e50e24dcca9e";

static constexpr char COMMAND_UUID[] =
    "6e400002-b5a3-f393-e0a9-e50e24dcca9e";

class CommandCallbacks : public BLECharacteristicCallbacks
{
public:

    CommandCallbacks(BLEManager& manager)
        : _manager(manager)
    {
    }

    void onWrite(BLECharacteristic* characteristic) override
    {
        std::string value = characteristic->getValue();

        if (value == "LAP")
        {
            _manager.setCommand(BLEManager::Command::Lap);
        }
        else if (value == "STOP")
        {
            _manager.setCommand(BLEManager::Command::Stop);
        }
    }

private:

    BLEManager& _manager;
};

bool BLEManager::begin()
{
    BLEDevice::init("MotorbikeLapLogger");

    BLEServer* server = BLEDevice::createServer();

    BLEService* service = server->createService(SERVICE_UUID);

    BLECharacteristic* commandCharacteristic =
        service->createCharacteristic(
            COMMAND_UUID,
            BLECharacteristic::PROPERTY_WRITE
        );

    commandCharacteristic->setCallbacks(
        new CommandCallbacks(*this)
    );

    service->start();

    BLEAdvertising* advertising =
        BLEDevice::getAdvertising();

    advertising->addServiceUUID(SERVICE_UUID);

    advertising->start();

    Serial.println("BLE started");
    Serial.println("Waiting for connection...");

    return true;
}

void BLEManager::setCommand(Command command)
{
    _command = command;
}

BLEManager::Command BLEManager::getCommand()
{
    Command command = _command;
    _command = Command::None;

    return command;
}