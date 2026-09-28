# Live motion system milestone

The current objective is a live motorcycle IMU system: lean, pitch, estimated
acceleration/deceleration and angular rates, with magnetic measurements evaluated
as useful. The final product must display live data in the existing iOS app without SD-card
storage, and GNSS is outside scope. The user clarified that development diagnostic
recordings remain useful and may be shared for analysis. These requirements
supersede the earlier logger roadmap. Recording is not required for normal use. Riding accuracy is not yet established.

The first successful motion capture, `nicla-motion-20260926-165741-469356.log`,
confirmed all four streams and valid post-calibration motion output after the
12.5 Hz correction. See its adjacent analysis report for observed hold windows.

## Available now

Use `--live-motion` (serial command `m`) for a 120-second live bench display without saving data:

| Output | Meaning | Status |
| --- | --- | --- |
| Lean and pitch | Right-positive roll, nose-up-positive pitch | Combined bench movements checked; absolute accuracy unverified |
| Forward acceleration | Estimated body-X linear acceleration in m/s² | New, pending successful hardware test; negative means acceleration toward the rear |
| Left and body-up acceleration | Estimated body-Y/Z linear acceleration in m/s² | New, pending successful hardware test; Z is bike-up, not earth vertical |
| Angular rates X/Y/Z | Rotation about forward, left, up axes in deg/s | Frame mapping bench checked; these are not Euler-angle derivatives |
| Accelerometer and gyro source values | Bosch-corrected virtual-sensor integer counts | 50 Hz, independent sequences and delivery times |
| Quaternion | Six-axis Game Rotation Vector XYZW Q14 | 50 Hz, raw norm checked before normalization |
| Magnetometer | Bosch-corrected virtual sensor 22 integer counts | New 12.5 Hz stream; units, axis mapping and magnetic environment not validated |

Magnetic measurements are not used in lean fusion at this stage. The nine axes
refer to the three accelerometer, three gyro and three magnetic components; they
do not provide nine independent orientation angles. Other environmental sensors
on the board are outside this motion milestone.

Negative longitudinal acceleration is consistent with deceleration when travelling
forward. Without velocity/direction and vehicle inputs it is not proof of braking,
and cannot distinguish brake use from other causes of slowing.

## Upload and first test

Close serial monitors, then upload:

```sh
~/.platformio/penv/bin/pio run -d /Users/amardeep/Desktop/MotorbikeLapLogger/Nicla/Nicla_Firmware -t upload
```

Display live without recording:

```sh
~/.platformio/penv/bin/python /Users/amardeep/Desktop/MotorbikeLapLogger/test_scripts/capture_nicla_serial.py --port /dev/cu.usbmodemCDC4CEFE2 --live-motion
```

Reset when prompted, put the enclosure upright (+Z up, +Y forward), and press
Enter. Rest it motionless on a surface for calibration and the initial baseline.
After calibration the tool prompts for gentle level forward/back movements,
another stationary period, a nose-up hold, and a leaned hold. Cable slack matters;
small movements are enough for a sign/sanity check. Handholding is acceptable for
this first check, but does not provide known acceleration or angles.

When moving forward from rest, expect a positive acceleration transient; stopping
that forward motion should produce a negative transient. Constant speed should
approach zero linear acceleration. Resting or holding a tilted pose still should
also approach zero, within sensor/orientation errors. Avoid interpreting a single
peak as measured brake performance.

`--live-motion` displays values and prompts without opening a data file. Existing
capture modes are retained only as optional diagnostics; they do save files and
are not the default workflow for the current product scope. The live bench run
still ends after 120 seconds; continuous wireless display remains future work.

The first motion attempt (2026-09-26 16:44) stopped at configuration validation:
sensor 22 reported 12.5 Hz after a 10 Hz request. Firmware now explicitly requests
and verifies 12.5 Hz. The later 16:57 capture confirmed the rate fix and complete acquisition. Do not interpret
the failed capture as a measurement or calibration failure.

## Calculation and quality fields

The current mounting calibration is a fixed 3D rotation. Apply it to the nominal
motorcycle-frame accelerometer and angular-rate vectors as well as the orientation
up vector. Compute:

`linear_acceleration = 9.80665 * (specific_force_g - estimated_up_unit_vector)`

Specific force retains the configured 4 g count scale; gyro retains the 1000 deg/s
scale. No accelerometer scale/bias fit, smoothing, velocity integration, or invented
precision correction is added. The existing upright calibration is an orientation
reference only, not a complete accelerometer calibration.

New `M` row fields:

```text
M,sequence,host_us,status,forward_mps2,left_mps2,up_mps2,wx_dps,wy_dps,wz_dps,acc_age_us,gyro_age_us,q_age_us,flags,calibration_id
```

Rows are emitted alongside `L` at 10 Hz. The six computed value fields are empty
when unavailable. Sources remain at 50 Hz (magnetic at 12.5 Hz) for later analysis.
These are latest delivered samples, not synchronized hardware samples.

- Flag 1: an accelerometer or gyro component is within 1% of its configured full
  range. Motion values are withheld; this is a conservative clipping-risk check.
- Flag 2: accelerometer magnitude differs from 1 g by more than 0.05 g. Values are
  retained with the flag. Its absence does not prove the device is stationary.
- Flag 4: source delivery ages differ by more than 40 ms. Motion values are withheld.
- Existing invalid quaternion, stale delivery (>100 ms), calibration and capture
  fault rules also apply. Queue loss and malformed events remain explicit counters.

`VALID` concerns available, internally acceptable measurements; it is not a
confidence score for physical accuracy. Sustained vehicle acceleration may bias
the fused gravity direction and therefore both lean and computed linear
acceleration. No claim of cornering compensation is made. Hardware timestamps
and upstream FIFO losses remain unavailable through the current wrapper.

Marker is `NICLA_MOTION_V1`. Raw rows retain the eight-field format; sensor IDs
4, 13, 22 and 37 have independent sequences. End counters include
`magnetometer_events`. No physical magnetic units or motorbike-frame remapping
are inferred from these newly enabled raw counts. Start/end rate, latency and
magnetic range readback must match. The recurring initial configuration timeout
is retried and recorded; root cause remains unresolved.

## Verification

- Nicla firmware build passed: static RAM 41,984 bytes, flash 311,996 bytes.
  Existing dependency warning in SensorClass remains.
- 31 Python capture/parser tests passed, including complete four-stream fixtures,
  missing events, missing END, finite values, stale/clipped data, guided motion
  prompts and raw-byte preservation.
- Native estimator tests passed, including stationary gravity cancellation at
  combined lean/pitch with mounting tilt, positive/negative forward acceleration,
  side/up components, angular-rate signs, range limits and delivery skew.
- The first hardware motion run failed configuration validation before measurement.
  The corrected 12.5 Hz configuration passed in the subsequent 16:57 capture.
  Static software tests cannot establish dynamic estimation accuracy.

## Roadmap to the live IMU system

1. **Measurement foundation (current):** validate lean/pitch, acceleration, angular
   rates, range limits, magnetic environment and timing. Use live status and
   repeatable physical checks; no routine capture/export requirement.
2. **Battery-powered live display:** the existing iPhone app is the chosen receiver.
   Add wireless calibration, live lean/pitch/motion, stale/disconnect handling and
   reconnection. No SD-card storage is required; development file capture/export remains acceptable. The [Bluetooth implementation](NICLA_BLUETOOTH_LIVE.md) is now built and ready
   for its first hardware connection test.
3. **Riding validation:** compare against independently known physical references,
   including vibration and sustained acceleration. Quantify accuracy and latency
   without claiming that an internally valid reading is necessarily accurate.
4. **Additional live channels:** evaluate turn rate, acceleration/deceleration
   indications and vibration. Magnetic heading requires calibration/interference
   checks. Do not promise speed, distance, position or GNSS-based lap timing.

GNSS and SD-card storage are outside the agreed scope. Do not add them without
a new user request. Temporary development recordings are explicitly permitted. Existing repository names and legacy ESP32 logging
code describe the earlier project, not current requirements.
