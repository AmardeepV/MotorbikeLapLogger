# Motorbike Lap Logger

A motorcycle lap-data logger built around an Arduino Nicla, ESP32, and an iPhone companion app.

The ESP32 receives IMU telemetry from the Nicla over UART, processes and logs telemetry to an SD card, manages sessions/laps, and publishes live display telemetry over Bluetooth Low Energy (BLE). The iPhone app displays live lean data and provides CAL, LAP, and STOP controls.

## Project Goal

- Live current lean angle
- Maximum left and right lean angles
- Acceleration data for future display and analysis
- Session and lap control
- Raw SD-card logging for debugging and post-session analysis
- Live telemetry visualization on an iPhone

The ESP32 remains the main processing and communication device. The SD card is intentionally retained as a complete raw-data/debugging record.

## Repository Structure

```text
MotorbikeLapLogger/
├── ESP32/
│   ├── src/
│   ├── lib/
│   ├── test/
│   └── platformio.ini
├── LapLogger/
│   ├── LapLogger.xcodeproj/
│   └── LapLogger/
│       ├── BluetoothManager.swift
│       ├── BluetoothView.swift
│       ├── ContentView.swift
│       ├── DashboardView.swift
│       ├── HistoryView.swift
│       ├── LapLoggerViewModel.swift
│       ├── SettingsView.swift
│       └── TelemetryView.swift
├── Nicla/
│   └── Nicla_Firmware/
└── test_scripts/
    ├── SESSION001.BIN
    ├── check_crc.py
    └── lap_reader.py
```

## Current Architecture

```text
Nicla ──UART──► ESP32
                 ├── PacketParser
                 ├── LapManager
                 ├── Button
                 ├── MetadataRecord
                 ├── CRC
                 ├── SDLogger
                 └── BLE telemetry/command interface
                       ├── Raw telemetry + metadata ──► SD card
                       └── Display telemetry ──BLE──► iPhone app
```

## Current Status

### Firmware and logging

- [x] Nicla → ESP32 UART communication
- [x] Fixed 50-byte telemetry packet and CRC-16
- [x] ESP32 packet parser
- [x] SD initialization and session file creation
- [x] Raw telemetry logging
- [x] Button-based lap/session control
- [x] 16-byte metadata records
- [x] SessionStart, LapStart, and SessionEnd metadata
- [x] Metadata CRC verification
- [x] Mixed telemetry + metadata binary log
- [x] Python binary reader and session/lap reconstruction

### Live telemetry and iPhone app

- [x] Lean-angle telemetry
- [x] Maximum left/right lean telemetry
- [x] BLE service and characteristic communication
- [x] iPhone BLE discovery and connection
- [x] BLE status messages
- [x] Live telemetry parsing
- [x] Landscape live dashboard
- [x] CAL, LAP, and STOP controls
- [x] Connection state and activity log
- [x] Saved-device reconnection
- [x] Automatic reconnection after connection loss or Bluetooth becoming available
- [x] Dashboard menu sheet

### Not yet completed

- [ ] Thorough real-world hardware and riding tests
- [ ] Verify physical left/right lean sign convention
- [ ] Validate maximum lean tracking during riding
- [ ] Add acceleration processing to the iPhone display
- [ ] Add historical/session visualization
- [ ] Improve PC-side log analysis/export

## iPhone BLE Telemetry Protocol

The iPhone currently expects this display-oriented packet:

```text
T1,<millis>,<signed-degrees>,<left-max-degrees>,<right-max-degrees>,<logging>
```

| Field | Meaning |
|---|---|
| `T1` | Telemetry packet version/type |
| `millis` | ESP32 source timestamp in milliseconds |
| `signed-degrees` | Current signed lean angle |
| `left-max-degrees` | Maximum left lean magnitude, non-negative |
| `right-max-degrees` | Maximum right lean magnitude, non-negative |
| `logging` | Logging state: `0` or `1` |

The iPhone validates packet structure, finite numeric values, maximum-angle ranges, and logging state before updating the dashboard.

## BLE UUIDs

| Component | UUID |
|---|---|
| Service | `6e400001-b5a3-f393-e0a9-e50e24dcca9e` |
| Command | `6e400002-b5a3-f393-e0a9-e50e24dcca9e` |
| Status | `6e400003-b5a3-f393-e0a9-e50e24dcca9e` |
| Telemetry | `6e400004-b5a3-f393-e0a9-e50e24dcca9e` |

The command characteristic is used for logger commands. Status and telemetry characteristics provide readable values and notifications to the iPhone.

## Automatic Reconnection

The iPhone stores the selected logger's peripheral UUID locally. It uses that UUID to attempt reconnection when:

- Bluetooth becomes available again
- The BLE connection is unexpectedly lost
- The saved logger is found during a subsequent scan

Reconnection behavior should still be tested with logger power cycling, Bluetooth toggling, and app background/foreground transitions.

## ESP32 Telemetry Packet

Telemetry packets are fixed at 50 bytes.

| Header field | Size | Value |
|---|---:|---|
| SOF1 | 1 | `0xAA` |
| SOF2 | 1 | `0x55` |
| Version | 1 | `0x01` |
| Length | 1 | `0x32` (50) |

Payload fields are: timestamp, `qw`, `qx`, `qy`, `qz`, `accelX`, `accelY`, `accelZ`, `gyroX`, `gyroY`, and `gyroZ`, each 4 bytes. The final 2 bytes contain the CRC, calculated over the packet excluding those final 2 bytes.

## Metadata Record

Metadata records are fixed at 16 bytes and are stored in the same `.BIN` file as telemetry packets.

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

Metadata magic is `0xCDAB`, appearing as `AB CD` in the little-endian binary file. Metadata types are `0x01` SessionStart, `0x02` LapStart, and `0x03` SessionEnd. The CRC is calculated over the first 14 bytes.

## Verified Test Log

The repository contains `test_scripts/SESSION001.BIN`.

- Verified file size: `170464` bytes
- 4 metadata records × 16 bytes = 64 bytes
- 3408 telemetry packets × 50 bytes = 170400 bytes
- Total = 170464 bytes

All four metadata CRCs were independently verified against the ESP32 CRC algorithm.

## ESP32 UART

```cpp
Serial2.begin(115200, SERIAL_8N1, 16, 17);
```

GPIO 16 is RX and GPIO 17 is TX.

## Session / Lap Behavior

- First short press starts Session 1 / Lap 1 and SD logging.
- Subsequent short presses finish the current lap and start the next lap while logging continues.
- Long press stops logging and closes the current session.
- The next session uses the next session number and a new `.BIN` file.
- The iPhone app can send the corresponding commands over BLE.

## Current Development Focus

The current implementation has reached a functional first iPhone dashboard and BLE reconnection milestone. The immediate focus is thorough testing before adding more features.

Test scenarios should include:

1. Normal BLE connection and telemetry updates.
2. CAL, LAP, and STOP commands.
3. Unexpected logger disconnection.
4. Logger power cycling.
5. iPhone Bluetooth being disabled and re-enabled.
6. App background/foreground transitions.
7. Stale or malformed telemetry.
8. SD logging and metadata integrity during operation.

## Development Approach

1. Preserve the working UART packet format unless there is a deliberate protocol change.
2. Preserve SD logging for debugging.
3. Keep the BLE display protocol versioned and documented.
4. Prefer small, testable components.
5. Verify changes on real ESP32/Nicla hardware when they affect timing, UART, SD, BLE, or sensor behavior.
6. Keep Python tools compatible with the binary format unless the format is intentionally versioned.
7. Test connection-loss and recovery behavior before relying on the system during riding.
