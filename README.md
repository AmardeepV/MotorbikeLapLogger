# Motorbike Lap Logger

A motorcycle lap-data logger built around an Arduino Nicla and ESP32. The current system receives IMU telemetry over UART, logs the raw binary stream to an SD card, and tracks sessions/laps with metadata records. The planned final system adds live lean-angle and acceleration display on an iPhone.

## Project Goal

The end goal is a live motorcycle telemetry display:

- Current left/right lean angle
- Maximum left lean
- Maximum right lean
- Acceleration data
- Lap/session information

The ESP32 is the main data-processing and communication device. The SD card remains enabled as a raw-data/debugging record so live display problems can be compared against recorded telemetry.

## Repository Structure

```
MotorbikeLapLogger/
├── ESP32/
│   ├── src/
│   ├── lib/
│   ├── test/
│   └── platformio.ini
├── Nicla/
│   └── Nicla_Firmware/
└── test_scripts/
    ├── SESSION001.BIN
    ├── check_crc.py
    └── lap_reader.py
```

## Current Architecture

```
Nicla
  │
  │ UART telemetry
  ▼
ESP32
  ├── PacketParser
  ├── LapManager
  ├── Button
  ├── MetadataRecord
  ├── CRC
  └── SDLogger
        │
        ▼
     SD card
     .BIN log

Future:
ESP32
  │
  └── BLE / Wi-Fi
          │
          ▼
        iPhone
```

## Current Status

- [x] Nicla → ESP32 UART communication
- [x] Fixed 50-byte telemetry packet
- [x] ESP32 packet parser
- [x] CRC-16 implementation
- [x] SD card initialization
- [x] Session file creation
- [x] Raw telemetry logging
- [x] Button-based lap/session control
- [x] 16-byte metadata records
- [x] SessionStart metadata
- [x] LapStart metadata
- [x] SessionEnd metadata
- [x] Metadata CRC verification
- [x] Mixed telemetry + metadata binary log
- [x] Python binary reader
- [x] Python session/lap reconstruction
- [ ] Lean-angle calculation on ESP32
- [ ] Verify physical left/right lean sign convention
- [ ] Maximum left/right lean tracking
- [ ] Acceleration processing for live display
- [ ] Define ESP32 → iPhone live-data protocol
- [ ] BLE or Wi-Fi transport
- [ ] iPhone live dashboard
- [ ] Historical/session visualization
- [ ] Robust PC-side log analysis/export

## ESP32 Telemetry Packet

Telemetry packets are fixed at 50 bytes.

Header:

| Field | Size | Value |
|---|---:|---|
| SOF1 | 1 | `0xAA` |
| SOF2 | 1 | `0x55` |
| Version | 1 | `0x01` |
| Length | 1 | `0x32` (50) |

Payload:

| Field | Size |
|---|---:|
| Timestamp | 4 bytes |
| qw | 4 bytes |
| qx | 4 bytes |
| qy | 4 bytes |
| qz | 4 bytes |
| accelX | 4 bytes |
| accelY | 4 bytes |
| accelZ | 4 bytes |
| gyroX | 4 bytes |
| gyroY | 4 bytes |
| gyroZ | 4 bytes |

The final 2 bytes contain the CRC. CRC is calculated over the packet excluding its final 2 CRC bytes.

## Metadata Record

Metadata records are fixed at 16 bytes and are stored in the same .BIN file as telemetry packets.

| Field | Size |
|---|---:|
| Magic | 2 |
| Type | 1 |
| Version | 1 |
| Timestamp | 4 |
| Session | 2 |
| Lap | 2 |
| Reserved | 2 |
| CRC | 2 |
| **Total** | **16** |

Metadata magic is `0xCDAB`, which appears as bytes `AB CD` in the little-endian binary file.

Types:

| Value | Type |
|---|---|
| `0x01` | SessionStart |
| `0x02` | LapStart |
| `0x03` | SessionEnd |

Metadata CRC is calculated over the first 14 bytes and stored in the final 2 bytes.

## Binary Log Layout

The .BIN file contains both record types sequentially:

```
[SessionStart - 16 bytes]
[Telemetry - 50 bytes]
[Telemetry - 50 bytes]
...
[LapStart - Lap 2 - 16 bytes]
[Telemetry - 50 bytes]
...
[LapStart - Lap 3 - 16 bytes]
[Telemetry - 50 bytes]
...
[SessionEnd - 16 bytes]
```

Metadata records are not required to be aligned to telemetry packet boundaries.

## Verified Test Log

The repository contains `test_scripts/SESSION001.BIN`.

Verified file size:

```
170464 bytes
```

The file contains:

- 4 metadata records × 16 bytes = 64 bytes
- 3408 telemetry packets × 50 bytes = 170400 bytes
- Total = 170464 bytes

Verified metadata offsets:

| Offset | Type | Meaning |
|---:|---|---|
| 0 | `0x01` | SessionStart |
| 63316 | `0x02` | LapStart, Lap 2 |
| 103232 | `0x02` | LapStart, Lap 3 |
| 170448 | `0x03` | SessionEnd |

All four metadata CRCs were independently verified against the same CRC algorithm used by the ESP32.

## ESP32 UART

The ESP32 currently uses:

```cpp
Serial2.begin(115200, SERIAL_8N1, 16, 17);
```

GPIO 16 is RX and GPIO 17 is TX.

## Session / Lap Behavior

Current intended behavior:

- First short press starts Session 1 / Lap 1 and starts SD logging.
- Subsequent short presses finish the current lap and start the next lap while logging continues.
- Long press stops logging and closes the current session.
- The next session uses the next session number and a new .BIN file.

## Important Design Decisions

### Keep raw SD logging

The SD card is intentionally retained even after live iPhone display is implemented. It provides a complete debugging record of the telemetry and session/lap events.

### Separate raw logging from live display data

The SD card should retain the complete raw telemetry stream. The future wireless interface does not need to transmit the complete raw packet if a smaller display-oriented packet is sufficient.

```
Raw telemetry ──► SD card
Processed display data ──► iPhone
```

This keeps the live wireless protocol simple while preserving enough data for debugging and later analysis.

## Next Development Step

The next implementation task is **lean-angle calculation on the ESP32**.

The ESP32 already receives quaternion values:

```
qw, qx, qy, qz
```

Before implementing the final formula, the physical orientation and coordinate convention of the Nicla mounting must be established. The result should then be tested by holding the sensor upright and tilting it deliberately left and right.

After the lean-angle calculation is validated:

1. Determine the physical sign convention for left/right lean.
2. Track maximum left and maximum right lean.
3. Process the acceleration values needed by the display.
4. Define a compact live-data packet.
5. Implement BLE or Wi-Fi communication from ESP32 to iPhone.
6. Build the iPhone dashboard.
7. Extend the Python tools for robust post-session analysis.

## Development Approach

When changing the firmware:

1. Preserve the existing working UART packet format unless there is a deliberate protocol change.
2. Preserve SD logging for debugging.
3. Prefer small, testable components.
4. Verify changes on real ESP32/Nicla hardware when they affect timing, UART, SD, or sensor behavior.
5. Keep the Python tools compatible with the binary format unless the format is intentionally versioned.
