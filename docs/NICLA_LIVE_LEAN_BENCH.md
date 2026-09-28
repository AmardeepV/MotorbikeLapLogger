# Live lean bench prototype

The `l` mode adds signed lean and nose-up-positive pitch to the existing raw
50 Hz accelerometer, gyro and Game Rotation Vector recording. Right lean is
positive; left lean is negative. Marked +Y is forward, +X right and +Z up.
This is a 120-second USB bench run, not a validated riding estimator.
The existing `s` and `q` diagnostics are still available.

## Upload and record

Close any serial monitor. Upload the updated firmware:

```sh
~/.platformio/penv/bin/pio run -d /Users/amardeep/Desktop/MotorbikeLapLogger/Nicla/Nicla_Firmware -t upload
```

Start the guided recording:

```sh
~/.platformio/penv/bin/python /Users/amardeep/Desktop/MotorbikeLapLogger/test_scripts/capture_nicla_serial.py --port /dev/cu.usbmodemCDC4CEFE2 --lean-test
```

If the device path changed, use the tool's `--list` option. Reset the board when
prompted, support the box truly upright and level, and press Enter. Leave it still
until `CALIBRATED` appears (normally about eight seconds). Calibration treats this
physical placement as upright; the sensors cannot verify that your support is level.
If calibration remains pending, improve the support rather than moving on.

Follow the prompts for right lean, front-up/front-down while maintaining that lean,
return upright, then the corresponding left-lean sequence. Aim for about 25 degrees
lean and 15 degrees pitch; exact angles are not required for this convention check.
Movement prompts are relative to successful calibration. Each movement has five
seconds to move and five seconds to hold. Return upright at the end and wait for
completion. Hand movement means changing lean cannot be attributed solely to
algorithm error; constrained motion and an independent reference are needed for
precision claims.

The terminal shows lean and pitch approximately twice per second. The file
`captures/nicla-lean-<timestamp>.log` retains raw events and 10 Hz angle/status rows.
Share the entire file, including failures. Reset before another recording. Calibration
is held in RAM and is lost on reset. No BLE telemetry or iOS changes are part of this
milestone.

## Calibration and validity

The estimator first waits five seconds for startup. It then requires at least
three seconds and 120 quaternion events meeting these bench acceptance criteria:

- Each of the three streams has a delivery age no greater than 100 ms.
- Quaternion norm is within 1% of unity before normalization.
- Acceleration magnitude is within 0.05 g of 1 g and gyro magnitude is below 1 deg/s.
- The nominal up direction is within 15 degrees of vertical and stays within
  0.5 degrees of the window's first accepted direction. A failed condition or
  quaternion delivery gap restarts the stable window.

These thresholds are engineering choices for the bench, not measured accuracy
bounds. The mean up vector defines a fixed, shortest 3D rotation to vertical.
That correction is applied to future up vectors before extracting roll and pitch.
It does not subtract two Euler offsets. Upright alone cannot determine mounting
yaw, so the confirmed forward-axis installation remains an assumption.

For normalized Hamilton XYZW, reported-frame up is
`(2(xz-wy), 2(yz+wx), 1-2(x²+y²))`. Convert it to the motorcycle frame with
`(-ux,-uy,uz)`, apply the mounting correction, then compute
`roll=atan2(up.y,up.z)` and `pitch=atan2(up.x,hypot(up.y,up.z))`.
The sign and direction convention is supported by the recorded roll/pitch tests;
synthetic tests exercise headings, but hardware yaw invariance remains untested.

Angle fields are blank during startup/calibration, stale delivery, invalid data,
or a latched capture fault. Roll is also withheld within five degrees of a vertical
forward axis, where this roll definition becomes singular. Malformed sensor packets
or raw queue overflow latch a fault until reset. A manual serial `c` command
invalidates the previous calibration and starts a new stable window; the guided
host tool instead expects one uninterrupted calibration per run.

The host tool stops the guided sequence if valid readings become unavailable,
rejects non-finite/invalid display rows, and reports stale data if no live status
arrives for one second. A late calibration that leaves insufficient time for the
full sequence is reported as incomplete. `Capture complete` means the recording
sequence ended; it does not certify measurement accuracy.

Host delivery time is not a hardware acquisition timestamp. Buffered upstream
events can appear freshly delivered. `VALID` means the data/calibration checks
passed, not that sustained cornering acceleration is accounted for. There is no
independent absolute-angle validation yet. The recurring first configuration-read
timeout is retained and retried; its root cause remains unresolved.

## Serial protocol

Marker: `# NICLA_LEAN_V1`. Raw sensor rows retain the eight-field quaternion capture
format, original Q14 quaternion values and independent per-sensor sequences.

Additional rows:

```text
L,host_us,status,roll_deg,pitch_nose_up_deg,calibration_id
L,5100000,CALIBRATING,,,0
L,8200000,VALID,0.012,-0.003,1
L,8500000,STALE,,,1
```

The first line above illustrates field names; firmware describes these in a `# LEAN`
comment. Status values are STARTUP, CALIBRATING, VALID, STALE, INVALID and
CAPTURE_FAULT. Angles have three decimal places for analysis, not an accuracy claim.
A successful recalibration increments the calibration ID. Live rows may precede
queued raw rows with earlier delivery times. Existing raw-capture analyzers reject
this distinct marker rather than silently ignoring the additional status records.

## Verification on 2026-09-26

- Firmware compiled for Nicla Sense ME: 41,952 bytes static RAM and 310,232 bytes
  flash. Existing Arduino library warnings remain.
- 26 Python capture tests passed, including preserved raw bytes, prompt timing,
  invalid/stale display handling and incomplete calibration/test sequences.
- Native C++ checks passed for combined lean/pitch across headings, mounting tilt,
  quaternion sign equivalence, invalid quaternion norms, stale streams, singular
  poses, calibration motion/acceleration rejection, recalibration and latched faults.
- Replayed the actual pitch log `nicla-quaternion-pitch-20260926-095747-318196.log`
  through the firmware estimator: calibration accepted at 8.020477 s, with no later
  invalid state. Initial calibrated roll (8.1–10 s) +0.0044 degrees, nose-up hold
  +0.3446 degrees, nose-down hold +0.1782 degrees, final +0.0101 degrees.
- Replayed `nicla-quaternion-roll-20260925-235307-878125.log`: calibration at
  8.011430 s; right hold +31.6325 degrees, left hold -28.8327 degrees, final
  -0.4815 degrees. The final physical-placement difference is retained.

Replay uses original callback ordering and delivery times; it does not test the
new firmware's on-board execution or serial throughput. Hardware upload and the
first new `--lean-test` capture are the next verification step.

To reproduce the native checks from the repository root:

```sh
c++ -std=c++11 -Wall -Wextra -Werror -I Nicla/Nicla_Firmware/lib/ImuFrames -I Nicla/Nicla_Firmware/lib/LeanEstimator test_scripts/test_lean_estimator.cpp -o /tmp/nicla-lean-tests
/tmp/nicla-lean-tests
python3 -m unittest discover -s test_scripts -p 'test_*capture.py' -q
c++ -std=c++11 -Wall -Wextra -Werror -I Nicla/Nicla_Firmware/lib/ImuFrames -I Nicla/Nicla_Firmware/lib/LeanEstimator test_scripts/replay_nicla_lean.cpp -o /tmp/nicla-lean-replay
/tmp/nicla-lean-replay captures/nicla-quaternion-pitch-20260926-095747-318196.log
```
