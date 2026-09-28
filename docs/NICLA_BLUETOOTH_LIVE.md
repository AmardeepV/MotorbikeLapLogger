# Live Nicla measurements in the iPhone app

Current baseline (2026-09-27): at the user's request, BLE debugging is paused.
The live build retains the first-press LAP fix, bounded lean-display gap handling,
duplicate-connection guards, calibration retention, iOS disconnect error details,
and upper-left Clear button. Later health/RSSI polling, health characteristic,
TX-credit hook/queued status mitigation, scheduling delay and isolation build
environments have been removed from the active path. The historical investigation
sections below describe earlier experiments, not the current configuration.
This is a reconstruction of the requested behavior, not an exact Git checkout:
that intermediate state was not committed. CALIBRATION_RETAINED remains the
accurate reconnect status. Radio stability is still unresolved.

The debugging source snapshot is preserved in
`captures/debug-checkpoints/ble-debug-before-live-restore.tar.gz`.
Upload `nicla_live_ble` and rebuild/run the iPhone app to apply this restoration.
Firmware and simulator builds plus estimator/protocol/display regression checks
passed; physical behavior requires uploading to the board and phone.

The Bluetooth build starts on battery power without a USB terminal. It streams
lean, pitch, estimated forward/left/body-up acceleration and three body angular
rates to the existing iOS app. No SD-card storage, persistent telemetry recording
or GNSS is used. Measurements are held in RAM for display. The saved Bluetooth
peripheral ID and the existing in-memory activity list are not session recordings.

## Install both sides

Close serial monitors. The project default is now **nicla_live_ble**, so the
normal project Upload action builds Bluetooth firmware. If the IDE has an explicit
environment selected, choose nicla_live_ble. This command selects it unambiguously:

```sh
~/.platformio/penv/bin/pio run -d /Users/amardeep/Desktop/MotorbikeLapLogger/Nicla/Nicla_Firmware -e nicla_live_ble -t upload
```

Open `/Users/amardeep/Desktop/MotorbikeLapLogger/LapLogger/LapLogger.xcodeproj`
in Xcode, select your iPhone, and build/run the updated app. Normal Xcode signing
and device trust requirements still apply. The old installed app cannot decode
the new live-motion characteristics, so both firmware and app need updating.

Disconnect the Nicla USB cable with the existing battery connected. Reset the
Nicla if needed. In the app's Bluetooth page, scan and connect to **Nicla Motion**.
If the app first tries an older saved ESP32, scan and choose Nicla Motion.

The existing app UI is preserved: dashboard, logger status, CAL, LAP, STOP,
and MAX keep their original layout. Do not redesign these screens without an
explicit request. Additional motion fields are received internally for future use.

Place the box upright (+Z up, +Y forward, +X right) and use the existing CAL control.
Keep it still until calibration completes. The startup settling period and a
three-second stable window apply; calibration times out after 30 seconds.

## First cable-free check

1. After calibration, lean left/right before LAP: live lean changes, MAX stays zero.
2. Press LAP: the session starts and MAX updates with valid lean readings.
3. Press LAP again: the lap advances; MAX retains the session's highest values.
4. Press STOP: MAX freezes while live lean continues.
5. Press LAP to start a new session: MAX resets and starts tracking again.

MAX is computed on the board at quaternion delivery rate, rather than from the
subset of packets received by the phone. Session state and maxima are sent together
in a repeated snapshot. Disconnect stops the active session. A completed calibration survives a radio
reconnect while the board remains powered; a board restart requires calibration. These are RAM-only controls, with no SD-card recording or GNSS.
Discovery updates entries by peripheral UUID and prefers the advertised local name;
distinct UUIDs are deliberately not merged just because their names match.

Both app and firmware must be uploaded again. Hardware checks remain necessary;
the coding agent has not installed or exercised this revision over a real BLE link.

## Data quality and lifecycle

Sensors run at 50 Hz for acceleration/gyro/Game RV and 12.5 Hz for magnetic counts.
The app receives three small notifications at approximately 10 Hz. Sensor delivery
freshness and range checks use the same estimator as the bench firmware. There is
no automatic accelerometer bias/scale correction beyond the existing mounting
orientation reference. Acceleration is experimental and depends on fused gravity;
sustained cornering remains unvalidated.

The app expires measurements after one second without a newer accepted packet
(the screen refreshes at least every 0.5 s). It clears measurements on disconnect,
Bluetooth-off, explicit recalibration, malformed core packets, and app foreground
entry. Duplicated or out-of-order packets cannot refresh stale values. Source
sequence and uptime counters use wrap-safe comparisons. A device reboot is
expected to disconnect; a new connection resets packet ordering.

A fresh board boot requires calibration; a radio reconnect retains a completed
reference and cancels an unfinished calibration. Sensor initialization/configuration failures
advertise a fault status and withhold values. Malformed sensor packets latch a
fault until restart. The calibration timeout is reported separately from BLE
write acknowledgement; a successful command write is not calibration completion.

Angular rates are available internally only when both packet sequence and source timestamp
match the current motion packet. Dropping one notification may briefly make the
rate data unavailable rather than mixing samples. Firmware notification writes
are not end-to-end delivery acknowledgements; missing sequence numbers are counted
by the app. Host delivery time still cannot prove sensor acquisition freshness or
expose upstream FIFO loss.

The user-confirmed mount mapping remains required. Upright calibration removes
small mounting tilt but cannot determine mounting yaw. Magnetic counts are sent
for future validation, but are not converted to heading or fused into lean. No
magnetic compass is claimed in the UI.

## Wire protocol (version 1)

Service: `6e400001-b5a3-f393-e0a9-e50e24dcca9e`.

| UUID suffix | Purpose |
| --- | --- |
| 002 | Write `CALIBRATE`, `LAP`, or `STOP` (ASCII, with response) |
| 003 | Read/notify ASCII status, at most 20 bytes |
| 005 | Read/notify 20-byte core motion packet, tag 0xA1 |
| 006 | Read/notify 20-byte angular/magnetic packet, tag 0xB1 |
| 007 | Read/notify 20-byte session/MAX snapshot, tag 0xC1 |

All integer fields are little-endian. Values use signed int16; 32767 denotes an
unavailable computed value. No packet fragmentation or large MTU is required.
The old 004 ASCII T1 lean characteristic is not published by this firmware; the
updated app still accepts it from legacy ESP32 firmware.

A1 layout: tag at 0; orientation status at 1; motion status at 2; motion flags at 3;
uint16 sequence at 4; uint32 boot milliseconds at 6; signed roll/pitch in 0.01° at
10/12; signed forward/left/body-up acceleration in 0.01 m/s² at 14/16/18.

Status numbers: 0 startup, 1 calibrating, 2 stale, 3 invalid, 4 sensor/capture fault,
5 valid, 6 waiting for an explicit calibration request. Motion flag bits retain
the bench definitions: 1 range-limit risk, 2 specific-force departure from 1 g,
4 excessive source delivery skew. Flags are quality information, not an independent
accuracy guarantee. Packet decimal resolution is not an accuracy specification.

B1 layout: tag at 0; valid flags at 1 (bit 0 angular rates, bit 1 magnetic counts);
uint16 sequence at 2; uint32 boot milliseconds at 4; signed body-X/Y/Z rates in
0.1°/s at 8/10/12; signed raw corrected magnetic X/Y/Z counts at 14/16/18.
Magnetic counts use sensor 22's reported frame and unvalidated units. They are not
necessarily in the same physical frame as the accelerometer/gyro.

## Software verification

- Bluetooth firmware builds with the LAP/STOP session implementation.
  The USB bench environment is unchanged by this revision.
- Updated iOS app builds for the iOS Simulator with Xcode 26.1.1; device signing,
  installation, visual device checks and a real BLE connection remain untested.
- C++ packet encoder and Swift decoder agree on golden binary packets, including
  signed values, unavailable sentinels and both measurement types.
- Swift ordering tests cover missing, duplicate and backwards packets, and counter
  wraparound. C++ estimator tests cover continuous operation and calibration across
  the roughly 71-minute microsecond-counter wrap.
- Existing 32 Python diagnostic capture tests pass. Upstream Arduino library and
  simulator service warnings remain; they did not prevent builds.

The USB tools remain available by explicitly uploading the `nicla_sense_me`
environment again. They do not communicate with the Bluetooth-only build.

### Session snapshot (007, C1)

20-byte little-endian packet: tag C1 at 0, active flag at 1, sequence at 2,
boot milliseconds at 4, session number at 8, current lap at 10, nonnegative
left/right MAX in hundredths of a degree at 12/14, reserved zero bytes 16–19.
The app joins this snapshot to A1 only when sequence and timestamp both match.
STOP sets current lap to zero and retains maxima. A new session resets maxima;
advancing laps does not. Calibration also clears maxima. Session numbers last
until board restart. Golden encoder/decoder tests and inactive/active/stopped
MAX transition tests cover this behavior.

## Calibration and display fixes (2026-09-27)

Calibration activity is latched from the CAL request through completion/failure;
stale, invalid, or temporarily absent motion packets cannot toggle the CAL button
or logger state back to idle. Repeated CAL commands do not restart the board's
stable window. The app retains the previous complete angle/MAX snapshot while
the next pair arrives, without refreshing the previous sample's age. Missing
partners still expire after one second. LAP uses the original app enable rule:
connected and not calibrating. STOP requires an active session.

BLE snapshots are queued and sent one characteristic at a time with sensor
servicing between writes. While calibrating, only core and session snapshots are
sent at 5 Hz; optional rate/magnetic notifications resume after calibration.
The board requests a 15–30 ms connection interval, negotiated by the phone.
ArduinoBLE can block while waiting for transmit capacity, so actual sensor timing
still requires a hardware check. Calibration thresholds have not been relaxed.

The existing activity history reports calibration hints once per second when the
reason changes: CAL_WAIT_DATA (missing/delayed samples), CAL_INVALID_SENSOR,
CAL_WAIT_UPRIGHT, CAL_WAIT_STILL (rotation), CAL_WAIT_GRAVITY (acceleration
magnitude outside the stationary window), or CAL_HOLD_STILL (continue holding
steady; this alone does not prove the entire stable window has completed).
CALIBRATION_TIMEOUT remains the final failure message. The reported start/timeout
sequence confirms the command reached the board; the exact hardware blocker has
not yet been established. No screen/layout changes were made for these fixes.

Regression checks cover calibration quality fluctuations and timeout, both BLE
packet arrival orders, missing-packet expiry, invalid data, and diagnostic hints.
Firmware and iOS simulator builds pass; cable-free calibration needs retesting
with both updated installations.

## LAP, brief gaps, reconnects, and Activity controls

LAP readiness now uses completed calibration, independent of the instantaneous
freshness of a sensor sample or whether the completion notification has been sent.
MAX still accepts only valid readings. After calibration, tilt freshness is based
on the quaternion; acceleration/rate output still requires all input streams to
be fresh and valid. An explicitly stale orientation packet may keep the previous
complete display sample for at most 300 ms from that sample's original receipt.
Invalid/fault/calibration states clear immediately; repeated stale packets cannot
extend the hold. Normal unmatched BLE pairs retain the existing one-second expiry.

Scanning no longer changes a live/connecting connection to scanning state. Saved
device discovery only initiates connection while scanning, and repeated connection
requests for the same active/pending device are ignored. Disconnect history now
includes the iOS error domain/code and description when available. A radio drop
stops the session but preserves completed board calibration; neither calibration
nor a lap is automatically started on reconnect. An unfinished calibration is
cancelled, and reboot still clears the RAM reference. These fixes remove identified
software failure paths; the cause of the reported physical radio interruptions is
not yet proven and requires a phone/board check.

Activity's Clear button is explicitly on the leading side of its navigation bar,
away from the top-right menu overlay. No other layout changes are included.
Both PlatformIO environments and the iOS simulator target build. Swift regression
tests cover bounded stale holds and recovery; C++ tests cover fresh tilt with late
acceleration/rate streams, retained calibration across gaps, and calibration reset.

## Persistent connection timeout investigation

The user still observes CBErrorDomain code 6 after duplicate-connection guards.
That confirms those guards did not resolve the physical link timeout. No claim
of a confirmed radio fix is made for the following diagnostic revision.

The installed Arduino mbed main loop does not sleep between sketch calls. The
installed ArduinoBLE HCICordioTransport runs its controller dispatcher on a
separate default-priority RTOS thread; HCI notification writes can wait for
controller buffers. The live sketch now sleeps 1 ms after every loop, including
early-return paths, and polls Bluetooth before reading BHY2. This is a scheduling
mitigation to test, not proof that scheduling caused the timeout. The connection
supervision timeout and sensor rates have not been increased or relaxed.

A new read-only characteristic 008 exposes a 20-byte D1 diagnostic packet:
version 1 at byte 1, saturating disconnect count at 2, board uptime milliseconds
at 4, lifetime maximum sensor-call microseconds at 8, BLE-poll/write microseconds
at 12, and loop-start gap microseconds at 16. All fields are little-endian.
Readout refreshes once per second on the board; the app requests it and RSSI every
five seconds while connected. It adds no notification stream and no disk storage.
Optional diagnostic read failures cannot invalidate live measurements.

Activity records an initial board summary on connection and the last available
summary, its receipt age, and last RSSI on disconnect. The disconnect error entry
remains. These measurements can identify large processing stalls and help compare
board uptime before/after a drop. A check from before a stall may miss it; reconnect
readout can capture lifetime peaks if the board did not restart. RSSI alone cannot
prove or exclude interference. Calibration retained across reconnect is now named
CALIBRATION_RETAINED instead of claiming a new calibration completed.

Both sides must be updated. Test with the phone nearby and screen awake for at
least the usual failure interval; if it drops, report the disconnect, Last link
check, and subsequent Board uptime entries. Compare later with background use
separately. Firmware/iOS builds and binary diagnostic encode/decode tests pass;
long-running physical radio validation remains outstanding.

Related upstream report (similar symptom, no verified root cause for this unit):
https://github.com/arduino/nicla-sense-me-fw/issues/125

## Screenshot evidence and transmit-credit guard

The supplied Activity screenshot shows uptime 91.6 -> 98.3 seconds and disconnect
count 4 -> 5, with calibration retained. This specific drop did not reboot the
board. The maximum sensor/BLE/loop times were already 104.4/713.2/714.9 ms before
that drop and remained identical afterward. These lifetime wall-time peaks are
not timestamped proof of causation; OS preemption also contributes to elapsed
call time. RSSI was unavailable, so the screenshot does not establish RF quality.

Installed ArduinoBLE 2.1.0 HCIClass::sendAclPkt waits without a bound while no
controller transmit credits are free. The new build checks controller credits
before application status and measurement notifications, defers without polling
inside the write, and reserves one credit for ATT/control traffic if capacity
permits. Status updates are coalesced to the latest state, and take priority over
measurement frames. The repeated session/motion snapshots remain authoritative.
The peripheral advertises for one connected central; the check assumes the one
notification recipient used by this firmware. Sensor rates and UI are unchanged.

The post-configuration PlatformIO script scripts/ble_tx_credit.py adds only a
read-only credit accessor to the environment-local HCI.h. It verifies the exact
original 2.1.0 header SHA-256, is idempotent, and fails for unknown source versions.
It does not change controller accounting, discard protocol responses, or break
out of upstream send loops. Internal ATT responses and HCI polling can still
block; this guard specifically prevents our notification producers from entering
the transmit-credit wait. Never replace it with an unconditional break in that
wait, which could send packets without controller capacity.

Health characteristic 008 now returns D2 (still 20 bytes): byte 0 D2, byte 1 drop
count saturated to 255, uint16 sensor maximum milliseconds at 2, uint32 uptime
milliseconds at 4, uint16 maximum poll/notify/status/loop milliseconds at 8/10/12/14,
uint32 deferral count at 16. Timing fields saturate at 65535 ms. The app retains
D1 compatibility and displays separate timing labels for D2. Deferrals count loop
attempts, not dropped packets. The increased resolution loss is acceptable for
identifying the observed hundreds-of-milliseconds stalls.

Validation: both updated targets build; C++/Swift D1/D2 golden packet tests pass;
credit-accessor tests exhaust all 65,536 combinations of buffer capacity and
pending count; the patch rejects unknown headers and is idempotent. Actual
connection stability remains unverified. Upload both sides, leave connected for
at least five minutes (the prior screenshot shows five drops within ~100 seconds),
and share a new disconnect/board summary if it recurs.

Upstream transmit implementation:
https://github.com/arduino-libraries/ArduinoBLE/blob/master/src/utility/HCI.cpp
Related report, not proof of this unit's root cause:
https://github.com/arduino-libraries/ArduinoBLE/issues/45

## First five-minute test with transmit-credit guard

User reports one disconnect during five minutes and no noticeable dashboard
interruption. Supplied Activity screenshot (10:02 phone display) records:

- Before drop: uptime 114.1 s, drops 0, lifetime maximum sensor/poll/notify/status/
  loop 1/0/0/0/6 ms, 73,510 TX deferrals, last RSSI -38 dBm. The health sample was
  received four seconds before the 09:57:51 disconnect (CBErrorDomain:6).
- Reconnected at 09:57:53: uptime 121.1 s, drops 1, maximum times 1/5/0/0/51 ms,
  76,420 deferrals. CALIBRATION_RETAINED follows at 09:57:55.
- A new LAP command was transmitted at 09:58:34.

Integer timing fields truncate sub-millisecond durations: 0 ms means less than
1 ms, not zero work. Deferrals are retry checks, not failed/dropped samples. The
board did not reboot. The recorded software stalls are substantially reduced
compared with the prior 713 ms BLE peak, consistent with the credit guard helping.
This does not establish the remaining timeout's cause or statistically quantify
reliability from one short run. Last RSSI does not exclude transient interference.

A real connection loss and reconnect occurred even though the user did not notice
a dashboard interruption. Existing behavior stops an active session on link loss;
reconnection preserves calibration but does not automatically restart a lap.
Do not describe recovery as uninterrupted measurement or a successfully continuous
session. Keep this improved build unchanged for the next 20-minute foreground,
nearby-phone bench observation, recording drop count and whether each drop occurs
only early in the run or recurs later. No additional upload is needed for that
observation. If drops recur, investigate controller disconnect reasons and actual
negotiated connection parameters; current app timeout and aggregate wall-time
measurements cannot uniquely determine the radio/controller cause.
