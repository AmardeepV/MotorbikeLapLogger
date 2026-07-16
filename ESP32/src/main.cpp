#include <Arduino.h>
#include "Button.h"

Button button(
    14,
    30,      // debounce
    3000);   // long press

void setup()
{
    Serial.begin(115200);

    button.begin();

    Serial.println();
    Serial.println("Motorcycle Lap Logger");
}

void loop()
{
    button.update();

    Button::Event event = button.getEvent();

    switch(event)
    {
        case Button::Event::ShortPress:
            Serial.println("Short Press");
            break;

        case Button::Event::LongPress:
            Serial.println("Long Press");
            break;

        default:
            break;
    }
}