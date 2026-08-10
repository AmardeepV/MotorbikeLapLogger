#include <Arduino.h>
#include <unity.h>

#include "LapManager.h"
#include "Button.h"

LapManager lapManager;

void setUp()
{
    // Runs before every test.
}

void tearDown()
{
    // Runs after every test.
}

void test_initial_state()
{
    TEST_ASSERT_FALSE(lapManager.isLogging());
    TEST_ASSERT_EQUAL_UINT32(0, lapManager.getCurrentLap());

    TEST_ASSERT_EQUAL(
        static_cast<int>(LapManager::Event::None),
        static_cast<int>(lapManager.getEvent())
    );
}

void test_first_short_press_starts_lap_one()
{
    lapManager.update(Button::Event::ShortPress, 1000);

    TEST_ASSERT_TRUE(lapManager.isLogging());
    TEST_ASSERT_EQUAL_UINT32(1, lapManager.getCurrentLap());

    TEST_ASSERT_EQUAL(
        static_cast<int>(LapManager::Event::LapStarted),
        static_cast<int>(lapManager.getEvent())
    );
}

void test_second_short_press_finishes_lap_one_and_starts_lap_two()
{
    lapManager.update(Button::Event::ShortPress, 1000);

    lapManager.update(Button::Event::ShortPress, 2500);

    TEST_ASSERT_TRUE(lapManager.isLogging());
    TEST_ASSERT_EQUAL_UINT32(2, lapManager.getCurrentLap());

    TEST_ASSERT_EQUAL_UINT32(
        1500,
        lapManager.getLastLapTime()
    );

    TEST_ASSERT_EQUAL(
        static_cast<int>(LapManager::Event::LapFinished),
        static_cast<int>(lapManager.getEvent())
    );
}

void test_multiple_short_presses_advance_laps()
{
    lapManager.update(Button::Event::ShortPress, 1000);
    lapManager.getEvent();

    lapManager.update(Button::Event::ShortPress, 2000);
    lapManager.getEvent();

    lapManager.update(Button::Event::ShortPress, 3500);

    TEST_ASSERT_TRUE(lapManager.isLogging());
    TEST_ASSERT_EQUAL_UINT32(3, lapManager.getCurrentLap());
    TEST_ASSERT_EQUAL_UINT32(1500, lapManager.getLastLapTime());

    TEST_ASSERT_EQUAL(
        static_cast<int>(LapManager::Event::LapFinished),
        static_cast<int>(lapManager.getEvent())
    );
}

void test_long_press_stops_logging()
{
    lapManager.update(Button::Event::ShortPress, 1000);
    lapManager.getEvent();

    lapManager.update(Button::Event::LongPress, 4000);

    TEST_ASSERT_FALSE(lapManager.isLogging());

    TEST_ASSERT_EQUAL_UINT32(
        0,
        lapManager.getCurrentLap()
    );

    TEST_ASSERT_EQUAL_UINT32(
        3000,
        lapManager.getLastLapTime()
    );

    TEST_ASSERT_EQUAL(
        static_cast<int>(LapManager::Event::LoggingStopped),
        static_cast<int>(lapManager.getEvent())
    );
}

void test_event_is_consumed()
{
    lapManager.update(Button::Event::ShortPress, 1000);

    TEST_ASSERT_EQUAL(
        static_cast<int>(LapManager::Event::LapStarted),
        static_cast<int>(lapManager.getEvent())
    );

    // getEvent() should consume the event.
    TEST_ASSERT_EQUAL(
        static_cast<int>(LapManager::Event::None),
        static_cast<int>(lapManager.getEvent())
    );
}

void test_long_press_when_idle_does_nothing()
{
    lapManager.update(Button::Event::LongPress, 1000);

    TEST_ASSERT_FALSE(lapManager.isLogging());
    TEST_ASSERT_EQUAL_UINT32(0, lapManager.getCurrentLap());

    TEST_ASSERT_EQUAL(
        static_cast<int>(LapManager::Event::None),
        static_cast<int>(lapManager.getEvent())
    );
}

void setup()
{
    delay(2000);

    UNITY_BEGIN();

    RUN_TEST(test_initial_state);
    RUN_TEST(test_first_short_press_starts_lap_one);
    RUN_TEST(test_second_short_press_finishes_lap_one_and_starts_lap_two);
    RUN_TEST(test_multiple_short_presses_advance_laps);
    RUN_TEST(test_long_press_stops_logging);
    RUN_TEST(test_event_is_consumed);
    RUN_TEST(test_long_press_when_idle_does_nothing);

    UNITY_END();
}

void loop()
{
}