# Nicla lean detector: first validation milestone

This branch is developing a Nicla-only motorcycle logger. The current executable
is a **stationary sensor diagnostic**, not a validated lean estimator. The legacy
ESP32 firmware and iPhone BLE protocol are retained for reference.

## What the repository review established

Reviewed the Nicla sensor, packet and battery libraries; ESP32 main program,
lean, lap, button, BLE, serial parser, CRC, metadata and SD libraries; existing
firmware tests and Python log readers; and the SwiftUI app and Bluetooth/session
handling. Generated build products and Xcode user-state files are not source
inputs to the design.

On 2026-09-25, GitHub `feature/Nicla-only` matched local HEAD
`caca478e8f454c7022216b4f55186c5289666685`. The 100-sample statistics experiment is
in that commit's `Nicla/Nicla_Firmware/src/main.cpp`; the working copy at review
time contained the subsequent gyro console experiment. No remote changes were
merged. Existing iOS working-copy edits were left untouched.

Confirmed findings:

1. The old statistics count every successful `OrientationSensor::update()`,
   including quaternion, Euler and gyro events. Such an event does not imply a
   new accelerometer measurement. The shared record mixes latest values from
   independently delivered streams.
2. Arduino_BHY2 1.0.8 `SensorXYZ` returns signed integer counts. Storing those in
   floats and printing two decimal places does not convert them into g or deg/s.
   IDs 4 and 13 are Bosch-corrected accelerometer/gyro virtual sensors. They are
   not the uncalibrated or passthrough sensor IDs.
3. The old setup enables sensors, calls `BHY2.update()`, then changes the ranges.
   Early values can precede that range change. A switch from an 8 g range to 4 g
   would change a stationary 1 g reading from about 4096 to 8192 counts. This is
   a **hypothesis for the half-magnitude readings**, not a demonstrated cause.
   The original code's Welford population-standard-deviation arithmetic is
   reasonable; the sample selection and startup conditions are the first concern.
4. `Serial.print(orientation.timestamp, 2)` uses **base 2** for that integer
   timestamp. It does not mean two decimal places. This complicates interpreting
   the historical console timestamps.
5. The old `2 * asin(qx)` expression is not a general roll calculation. In
   particular q and -q represent the same orientation but this expression can
   report different signs. Mounting orientation and quaternion conventions must
   be established before replacing it.
6. Rotation Vector (ID 34) and Orientation (ID 43) use magnetometer fusion.
   Omitting an explicit magnetometer object did not make the old experiment
   magnetometer-free. Game Rotation Vector (ID 37) is the later six-axis candidate.
7. Default `BHY2.begin()` starts the library's BLE/I2C services. The diagnostic
   explicitly uses `NICLA_STANDALONE` and enables only IDs 4 and 13.
8. `SensorXYZ` stores the last event only. Multiple arrivals within one
   `BHY2.update()` can overwrite intermediate values before the loop reads them.
   The new capture subclass records each `setData()` callback in a bounded queue.
9. The library removes BHI FIFO timestamps before delivering `SensorDataPacket`.
   Our `host_us` is therefore host delivery time. It cannot measure precise
   acquisition jitter or support rigorous gyro integration. Preserving hardware
   timestamps and meta events is a later prerequisite for dynamic validation.

Other migration concerns recorded for later work:

- The iPhone validates six-field `T1` messages and considers telemetry stale after
  two seconds. ESP32 currently sends at most 10 Hz. Preserve UUIDs, command strings,
  signed lean convention and existing status strings during migration.
- `didWriteValueFor` confirms a BLE write, not completed command execution.
  Session/calibration UI state is driven by status messages. Reconnection will
  need a state snapshot, because the current app does not reconstruct state from
  telemetry alone.
- iPhone Activity is an in-memory event list, not persistent raw lap recording.
  Removing the SD card needs an explicit storage/disconnection strategy.
- The ESP32 lap tests still call the old `Button::Event` API, while `LapManager`
  now accepts `LapManager::Command`. The suite also shares mutable state across
  tests. These legacy tests are not evidence of current lap-manager correctness.
- SD filenames use a source timestamp; collision/append behavior after restarts
  needs review if that architecture remains in use. The Python lap reader does
  not validate every telemetry CRC. The independent CRC script checks metadata.
- Battery presence inferred from voltage is not yet validated. Battery code is
  not invoked by this USB experiment and has not been redesigned here.

## Why nine axes do not guarantee accurate motorcycle lean

The board provides three acceleration axes, three angular-rate axes, and three
magnetic-field axes (BHI260AP plus BMM150). An accelerometer measures specific
force, not an isolated gravity vector. Braking and sustained cornering can make
accelerometer-derived tilt misleading. Gyros help track rapid rotation but have
bias and drift; a magnetometer introduces a different reference that can be
disturbed by the motorcycle. A smoother signal alone is not evidence of accuracy.

Initially define lean as motorcycle body roll relative to local vertical, using a
documented body frame: X forward, Y left, Z up. Validate positive rotation with
the right-hand rule and match the UI's left-negative/right-positive convention. Road-relative
lean requires knowledge of road banking; a board alone does not directly provide
that. Board +Y appears to point forward, but the full mounting transform and signs
remain unverified. Do not hard-code the tentative six-position mapping yet.

Accuracy must be measured against an independent reference. Future tests should
report static error at known angles, pitch/yaw coupling, dynamic RMS/95th-percentile
and worst-case error, latency, drift over a session, and recovery after disturbances.
Numerical acceptance targets remain to be agreed; no accuracy claim follows from
this firmware compiling. If on-board fusion fails sustained-turn tests, consider
vehicle constraints and independent speed/GNSS/reference measurements rather than
merely increasing smoothing.

## Experiment 1: one stationary capture

Purpose: determine whether the half-magnitude events are startup-only, verify
configuration and delivery rate, and measure the stationary residual gyro mean.
This is not yet a six-face calibration or a gyro-bias correction.

1. Build/upload `Nicla/Nicla_Firmware` using its `nicla_sense_me` environment.
   The executable is now the diagnostic; it does not transmit lap telemetry.
2. Place the secured enclosure on a stable table, away from vibration, and keep
   the USB cable slack. Record which marked axis points upward. Leave it untouched
   throughout the capture, including startup.
3. Open the serial monitor at 115200 baud and enable logging **before** sending
   lowercase `s`. The firmware waits for this command even if the initial prompt
   was missed when the monitor connected. Do not enable timestamps/prefixes on
   individual lines: the file parser expects the firmware output unchanged.
4. Capture until `# END,...` and the reset instruction appear, about 35 seconds
   after sensor setup. Preserve all `#` metadata and all data rows. The first
   five seconds are retained as startup; the next 30 are the stationary window.
   Five seconds is an analysis boundary, not a claim of complete thermal settling.
5. Save one run per log. Reset before another capture. Collect three runs in
   the same position before moving on to other faces.

From the repository root, with `pio` available on PATH:

```sh
pio run -d Nicla/Nicla_Firmware
pio run -d Nicla/Nicla_Firmware -t upload
pio device monitor -d Nicla/Nicla_Firmware -b 115200 --filter log2file
```

The monitor reports where it saves its log. Use that path with:

```sh
python3 test_scripts/analyze_nicla_capture.py /absolute/path/to/capture.log
```

### Save directly to a file without copying from the monitor

Close the PlatformIO/Arduino serial monitor, then run:

```sh
~/.platformio/penv/bin/python test_scripts/capture_nicla_serial.py
```

The tool opens the serial connection and asks you to reset the board and position
the box. Press Enter once it is motionless. The tool sends `s` for you, saves the
original serial bytes to a new timestamped file in `captures/`, and stops at
`# END` or a firmware error. A 90-second timeout preserves incomplete logs.
It prints the absolute filename to share. Existing files are never overwritten.
If several devices are present, use `--list`, then `--port /dev/cu.YOUR_DEVICE`.
No firmware change or SD card is required. These files are stored on the computer.

An exit status of 2 means the capture needs review (including a missing end marker,
wrong configuration, sequence gaps, queue overflow or unexpected sample count).
It is not a reason to remove the problematic readings. Each sensor should deliver
about 3000 events during the 30-second stationary window; the analyzer's +/-5%
count tolerance is a diagnostic check, not a specification of sensor accuracy.

## Data and limitations

CSV rows: `sensor_id,sequence,host_us,x_counts,y_counts,z_counts`.
Sequences count delivered callbacks independently for IDs 4 and 13. Equal XYZ
values are valid distinct measurements and are not deduplicated. A 256-event
application queue preserves bursts; a full queue drops the new event, increments
`queue_dropped`, and leaves a sequence gap. Malformed packets are counted.
Serial writes occur outside callbacks, one compact row between sensor polls.

Ranges are requested before enabling measurements. Both start and end readbacks
must match 100 Hz, zero requested latency, +/-4 g and +/-1000 deg/s. Readbacks use
the library's parameter API with explicit transport/length checks, because its
convenience `getConfiguration()` discards those errors. The BHI firmware may
reject/round settings; report an error rather than silently assume they worked.
Failed read transactions are retried up to three times, servicing sensor events
and waiting 10 ms between attempts. Each failure records its phase, sensor ID,
attempt, Bosch status code and returned byte count as `# CONFIG_READ_FAILURE`.
These remain visible in the analysis even when a retry succeeds. A valid response
with incorrect settings is not retried or accepted. Exhausted retries stop capture.
The first hardware attempt failed accelerometer readback while gyro readback
succeeded; the original message omitted status/length, so its cause is unresolved.

The nominal conversion for the signed 16-bit stream is count * range / 32768.
At 4 g that gives 8192 counts/g; at 1000 deg/s it gives about 32.768 counts/(deg/s).
For example 3530 gyro counts is about 107.7 deg/s at that verified range, not
3530 deg/s. This is nominal scaling, not measured scale-factor calibration.
The analyzer converts only the stationary window with matching readbacks; startup
remains in counts because transient behavior is precisely what we are studying.

The report retains every valid row and separately reports startup and stationary
statistics, vector magnitude, residual gyro mean, delivery gaps, rail hits and
low-magnitude events. A low-magnitude threshold of 75% of the stationary median
only locates evidence. Rail hits do not detect every kind of upstream saturation.

Limits: events lost inside the BHI FIFO are not counted by our application queue;
hardware timestamps, sensor-error meta events, bandwidth, temperature and
calibration accuracy are not yet exposed. Zero application drops cannot prove
zero upstream loss. Start/end range checks cannot identify the exact time of an
unexpected mid-run range transition. No filtering, axis remapping, bias subtraction
or magnetometer readings are introduced in this experiment.

## Next gates

1. Inspect three stationary logs, especially the first samples and low magnitudes.
2. Repeat all six faces with the verified data path; document board/enclosure axes.
3. Measure longer stationary gyro runs and controlled rotations in both directions.
4. Preserve BHI timestamps/status and establish sampling/bandwidth/saturation
   behavior before dynamic integration or fusion comparisons.
5. Compare full-quaternion six-axis and nine-axis estimates on a reference fixture,
   then investigate vehicle acceleration and vibration effects.
6. Add mounting calibration and lean extraction only after the frame conventions
   are verified. Add Nicla BLE/session/storage behavior after measurement validation.

## Software verification for this milestone

- Nicla firmware compiled with Nordic nRF52 platform 11.0.0, Arduino mbed 4.6.0,
  Arduino_BHY2 1.0.8 and ArduinoBLE 2.1.0. Existing dependency warnings remain.
- Nine Python regression tests exercise repeated values, startup outliers,
  missing rows, truncation, overflow reporting, invalid ranges/counts, delivery
  bursts, and multiple-run rejection. These use synthetic data, not board data.
- A native C++ test verifies queue burst ordering, wraparound and overflow counts.
- No hardware capture, orientation accuracy, or riding performance has been
  verified by these software checks.

Run the regression checks from the repository root:

```sh
python3 -m unittest discover -s test_scripts -p test_nicla_capture.py -v
c++ -std=c++11 -Wall -Wextra -Werror -I Nicla/Nicla_Firmware/lib/DiagnosticCapture test_scripts/test_capture_buffer.cpp -o /tmp/nicla-capture-buffer-test
/tmp/nicla-capture-buffer-test
```

## Primary references

- [Arduino Nicla hardware](https://docs.arduino.cc/hardware/nicla-sense-me/):
  BHI260AP six-axis sensor hub and BMM150 magnetometer.
- [Bosch BHI260AP datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bhi260ap-ds000.pdf):
  sections 13.2.8 (range changes), 13.3 (parameters), 15 (virtual sensors and
  formats), 15.2 (timestamps), and 19.4 (nominal sensor characteristics).
- [Arduino_BHY2 1.0.8 sources](https://github.com/arduino-libraries/Arduino_BHY2/tree/1.0.8/src):
  `SensorXYZ.h`, `SensorID.h`, `SensorManager.cpp`, `SensorClass.cpp`,
  `BoschParser.cpp`, `BoschSensortec.cpp`, and `Arduino_BHY2.cpp`.
