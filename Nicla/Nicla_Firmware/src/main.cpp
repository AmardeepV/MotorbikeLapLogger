#include <Arduino.h>
#ifdef NICLA_LIVE_BLE
#include "LiveBluetooth.h"
#include "NiclaBattery.h"
NiclaBattery battery;

void setup() { 
    Serial.begin(115200);

    if (!battery.begin())
    {
        Serial.println("Battery initialization failed");
    }
    else
    {
        Serial.println("Battery initialized");
    }   
    LiveBluetooth::begin(); 
    
}
void loop() { LiveBluetooth::update(); 
}
#else
#include "SensorDiagnostics.h"
void setup() { Serial.begin(115200); SensorDiagnostics::begin(); }
void loop() { SensorDiagnostics::update(); }
#endif
