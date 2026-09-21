
#include "BLEManager.h"

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

static constexpr char SERVICE_UUID[] =
    "6e400001-b5a3-f393-e0a9-e50e24dcca9e";

static constexpr char COMMAND_UUID[] =
    "6e400002-b5a3-f393-e0a9-e50e24dcca9e";

static constexpr char STATUS_UUID[] =
    "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

static BLECharacteristic* statusCharacteristic = nullptr;

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
        else if (value == "CALIBRATE")
        {
            _manager.setCommand(BLEManager::Command::Calibrate);
        }
    }

private:
    BLEManager& _manager;
};

class ServerCallbacks : public BLEServerCallbacks
{
public:
    ServerCallbacks(BLEManager& manager)
        : _manager(manager)
    {
    }

    void onConnect(BLEServer* server) override
    {
        _manager.onConnected();
    }

    void onDisconnect(BLEServer* server) override
    {
        _manager.onDisconnected();
    }

private:
    BLEManager& _manager;
};

bool BLEManager::begin()
{
    BLEDevice::init("MotorbikeLapLogger");

    BLEServer* server = BLEDevice::createServer();

    server->setCallbacks(
        new ServerCallbacks(*this)
    );

    BLEService* service = server->createService(SERVICE_UUID);

    BLECharacteristic* commandCharacteristic =
        service->createCharacteristic(
            COMMAND_UUID,
            BLECharacteristic::PROPERTY_WRITE
        );

    statusCharacteristic =
        service->createCharacteristic(
            STATUS_UUID,
            BLECharacteristic::PROPERTY_READ |
            BLECharacteristic::PROPERTY_NOTIFY
        );

    statusCharacteristic->addDescriptor(
        new BLE2902()
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

void BLEManager::sendStatus(const String& status)
{
    if (statusCharacteristic == nullptr)
    {
        return;
    }

    if (!_connected)
    {
        Serial.print("Cannot send BLE status: ");
    }
    else
    {
        statusCharacteristic->setValue(status.c_str());
        statusCharacteristic->notify();

        Serial.print("BLE status sent: ");
    }

    Serial.println(status);
}

void BLEManager::onConnected()
{
    _connected = true;

    Serial.println("BLE client connected");

    sendStatus("CONNECTED");
}

void BLEManager::onDisconnected()
{
    _connected = false;

    Serial.println("BLE client disconnected");

    Serial.println("Restarting BLE advertising...");

    BLEDevice::startAdvertising();
}

bool BLEManager::isConnected() const
{
    return _connected;
}