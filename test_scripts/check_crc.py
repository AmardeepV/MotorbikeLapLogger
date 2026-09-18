def calculate_crc(data):
    crc = 0xFFFF

    for byte in data:
        crc ^= byte << 8

        for _ in range(8):
            if crc & 0x8000:
                crc <<= 1
                crc ^= 0x1021
            else:
                crc <<= 1

            crc &= 0xFFFF

    return crc


with open("SESSION001.BIN", "rb") as f:
    data = f.read()


i = 0

while True:
    i = data.find(b"\xAB\xCD", i)

    if i == -1:
        break

    record = data[i:i + 16]

    # Make sure we actually have a complete record
    if len(record) == 16:
        calculated = calculate_crc(record[:14])
        stored = int.from_bytes(record[14:16], "little")

        type_byte = record[2]

        print(f"Offset:     {i}")
        print(f"Type:       0x{type_byte:02X}")
        print(f"Stored CRC: 0x{stored:04X}")
        print(f"Calc CRC:   0x{calculated:04X}")

        if calculated == stored:
            print("CRC:        OK")
        else:
            print("CRC:        FAILED")

        print()

    i += 1
