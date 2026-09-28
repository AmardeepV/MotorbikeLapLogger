# Six-axis quaternion bench validation

This step captures Bosch Game Rotation Vector (ID 37) alongside corrected
accelerometer (4) and gyro (13). It does not yet calculate or transmit lean.
The original `s` command remains the 35-second, 100 Hz vector diagnostic.
The new `q` command captures all three streams for 60 seconds at 50 Hz.

## Why this sensor

Bosch documents Game Rotation Vector as accelerometer/gyro fusion, whereas the
ordinary Rotation Vector uses a magnetometer too. This experiment requests only
IDs 4, 13 and 37 and starts BHY2 in standalone mode. Magnetometer fusion is not
enabled. Game RV has no absolute magnetic heading reference; yaw drift remains
possible. Nothing here establishes accuracy under motorcycle acceleration.

The [BHI260AP datasheet](https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bhi260ap-ds000.pdf)
describes the sensor dependencies in Table 14 and the Quaternion+ payload in
section 15.1.1. XYZW are signed 16-bit values scaled by 2^-14; the unsigned
accuracy field is reported as zero for Game RV, not as proof of zero error.
The capture retains these five raw fields without normalization or axis changes.

## First run: motorcycle roll on the bench

1. Upload the updated `Nicla/Nicla_Firmware` project. This step requires one
   firmware upload because the preceding firmware only understood `s`.
2. Close other serial monitors. With the known current port, run:

```sh
~/.platformio/penv/bin/python /Users/amardeep/Desktop/MotorbikeLapLogger/test_scripts/capture_nicla_serial.py --port /dev/cu.usbmodemCDC4CEFE2 --quaternion-test roll
```

3. Reset when prompted. Start with marked +Z up, marked +Y pointing forward
   away from you, and marked +X to your right, matching the confirmed mounting.
4. Use a support or guide for each hold. Leave cable slack. Press Enter to start.
   The tool sends `q`, follows the arriving host timestamps, and displays cues.

| Time | Action |
|---|---|
| 0–10 s | Upright and still |
| 10–15 s | Slowly lean the top right, toward marked +X, about 30 degrees |
| 15–22 s | Hold right lean |
| 22–27 s | Return upright |
| 27–32 s | Hold upright |
| 32–37 s | Slowly lean the top left, toward marked −X, about 30 degrees |
| 37–44 s | Hold left lean |
| 44–49 s | Return upright |
| 49–60 s | Hold upright until finished |

Lean about the **forward/back axis, marked Y**. Do not repeat the earlier
marked-X quarter-turn. About 30 degrees is sufficient for convention validation;
without a measured angle reference this cannot quantify absolute angle error.
If movements run late, complete them smoothly and report that fact with the log.
The analysis will use observed motion rather than assuming exact prompt timing.

The saved file is `captures/nicla-quaternion-roll-<timestamp>.log`.
Share the full file including configuration and errors. The tool preserves
incomplete files. A timeout with no quaternion marker can mean the old firmware
is still installed or the board was not reset to its command-waiting state.

## Later runs

After interpreting the first roll run, use `--quaternion-test pitch` to check
pitch-only behavior. It has the same timing but asks for nose-up then nose-down,
about 20 degrees, rotating about marked X. The front is marked +Y. The purpose
is to check that correctly extracted roll does not mistake pitch for lean.
`--quaternion-test upright` is available for a full stationary minute if needed.
Each run requires a reset, not another firmware upload.

## Data format and software checks

Marker: `# NICLA_QUATERNION_V1`

Rows: `sensor_id,sequence,host_us,x_raw,y_raw,z_raw,w_raw,accuracy_raw`

- IDs 4 and 13 retain signed sensor counts, with trailing w/accuracy set to zero.
- ID 37 contains signed Q14 XYZW plus unsigned accuracy. Divide XYZW by 16384.
- Each event has its own sequence and host delivery timestamp. No synchronization
  between streams or hardware acquisition timestamp is claimed.
- The shared queue retains callbacks arriving in a burst, with explicit overflow
  and malformed counters. Configuration readback verifies 50 Hz and zero latency
  on all streams, with 4 g and 1000 deg/s on vector sensors. No quaternion range
  is set. Start/end settings are compared by the analyzer.
- 50 Hz keeps the three-stream serial data rate comfortably below 115200-baud
  payload capacity for these short rows. It is a bench setting, not a final riding
  sampling-rate or bandwidth choice.

Check the saved file with:

```sh
python3 test_scripts/analyze_nicla_quaternion.py /absolute/path/to/capture.log
```

This reports continuity, count/rate checks, configuration errors, quaternion norm
before normalization, and sign flips. Its 1% norm tolerance is a diagnostic gate,
not an angle-accuracy bound. q and −q represent the same rotation, so sign flips
alone are not angle discontinuities. Moving periods are not used as stationary
bias measurements. No mounting transform or lean-angle formula is applied yet.

The first real log must establish which quaternion rotation direction and frame
match the static acceleration vectors and known movements. The vector conversion
in `ImuFrames.h` cannot simply be applied to quaternion XYZ; orientation must be
composed with the installation rotation in the correct order after its convention
is established. Hardware timestamps/status and dynamic validation remain future
work. The recurring initial readback failure remains recorded, not root-caused.

Software verification: firmware build succeeded (41840 bytes reported static RAM,
306260 bytes flash); 23 Python tests and the native capture-buffer test passed.
Existing upstream compiler warnings remain. Roll and pitch hardware captures have since been evaluated; their reports are
retained beside the logs in `captures/`. The next stage is the
[live lean/pitch prototype](NICLA_LIVE_LEAN_BENCH.md).
