#include <Arduino.h>

void setup()
{
    Serial.begin(115200);

    while(!Serial)
    {
    }

    Serial.println("Nicla Started");
}

void loop()
{

}