#include <Arduino.h>
#include <unity.h>

#include "PacketParser.h"

static const uint8_t validPacket[] =
{
    0xAA, 0x55, 0x01, 0x32,
    0xB0, 0xB7, 0x0B, 0x00,
    0xD5, 0xF7, 0x7F, 0x3F,
    0xE6, 0xFF, 0x1B, 0x3B,
    0xD6, 0xFF, 0xF7, 0xBB,
    0xE9, 0xFF, 0x09, 0xBC,
    0x00, 0x00, 0xF2, 0x42,
    0x00, 0x00, 0x2C, 0x42,
    0x00, 0x5C, 0x00, 0x46,
    0x00, 0x00, 0x00, 0x40,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x53, 0x80
};

static constexpr size_t PACKET_LENGTH = sizeof(validPacket);

void setUp()
{
}

void tearDown()
{
}

void feedPacket(PacketParser& parser,
                const uint8_t* packet,
                size_t length)
{
    for (size_t i = 0; i < length; i++)
    {
        parser.processByte(packet[i]);
    }
}

void test_valid_packet_is_accepted()
{
    PacketParser parser;

    bool packetParsed = false;

    for (size_t i = 0; i < PACKET_LENGTH; i++)
    {
        if (parser.processByte(validPacket[i]))
        {
            packetParsed = true;
        }
    }

    TEST_ASSERT_TRUE(packetParsed);
}

void test_valid_packet_decodes_timestamp()
{
    PacketParser parser;

    feedPacket(parser, validPacket, PACKET_LENGTH);

    const auto& data = parser.getTelemetry();

    TEST_ASSERT_EQUAL_UINT32(767920, data.timestamp);
}

void test_valid_packet_decodes_orientation()
{
    PacketParser parser;

    feedPacket(parser, validPacket, PACKET_LENGTH);

    const auto& data = parser.getTelemetry();

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.00f, data.qw);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.00f, data.qx);
}

void test_valid_packet_decodes_acceleration()
{
    PacketParser parser;

    feedPacket(parser, validPacket, PACKET_LENGTH);

    const auto& data = parser.getTelemetry();

    TEST_ASSERT_FLOAT_WITHIN(0.01f, 121.0f, data.accelX);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 43.0f, data.accelY);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 8215.0f, data.accelZ);
}

void test_corrupted_crc_is_rejected()
{
    PacketParser parser;

    uint8_t corruptedPacket[PACKET_LENGTH];

    memcpy(
        corruptedPacket,
        validPacket,
        PACKET_LENGTH
    );

    // Change one payload byte.
    corruptedPacket[10] ^= 0x01;

    bool packetParsed = false;

    for (size_t i = 0; i < PACKET_LENGTH; i++)
    {
        if (parser.processByte(corruptedPacket[i]))
        {
            packetParsed = true;
        }
    }

    TEST_ASSERT_FALSE(packetParsed);
}

void test_wrong_sof_is_rejected()
{
    PacketParser parser;

    uint8_t corruptedPacket[PACKET_LENGTH];

    memcpy(
        corruptedPacket,
        validPacket,
        PACKET_LENGTH
    );

    corruptedPacket[0] = 0xAB;

    bool packetParsed = false;

    for (size_t i = 0; i < PACKET_LENGTH; i++)
    {
        if (parser.processByte(corruptedPacket[i]))
        {
            packetParsed = true;
        }
    }

    TEST_ASSERT_FALSE(packetParsed);
}

void test_parser_can_recover_after_garbage()
{
    PacketParser parser;

    uint8_t garbage[] =
    {
        0x12,
        0x34,
        0x99,
        0xAA
    };

    for (uint8_t byte : garbage)
    {
        parser.processByte(byte);
    }

    bool packetParsed = false;

    // Feed the valid packet.
    for (size_t i = 0; i < PACKET_LENGTH; i++)
    {
        if (parser.processByte(validPacket[i]))
        {
            packetParsed = true;
        }
    }

    TEST_ASSERT_TRUE(packetParsed);
}

void setup()
{
    delay(2000);

    UNITY_BEGIN();

    RUN_TEST(test_valid_packet_is_accepted);
    RUN_TEST(test_valid_packet_decodes_timestamp);
    RUN_TEST(test_valid_packet_decodes_orientation);
    RUN_TEST(test_valid_packet_decodes_acceleration);
    RUN_TEST(test_corrupted_crc_is_rejected);
    RUN_TEST(test_wrong_sof_is_rejected);
    RUN_TEST(test_parser_can_recover_after_garbage);

    UNITY_END();
}

void loop()
{
}