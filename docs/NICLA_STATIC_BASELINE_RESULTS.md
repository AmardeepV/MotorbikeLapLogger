# Supported static baseline — 2026-09-25

Analysis uses each capture's 5–35 s window. Units are nominal full-scale
conversions of Bosch-corrected virtual sensors 4 and 13. This validates a bench
baseline, not absolute lean accuracy, raw uncalibrated sensor behavior, or riding
performance. No calibration constants have been applied.

## Selected recordings

All paths below are relative to `captures/`.

| Marked axis upward | Selected log | Mean acceleration magnitude (g) |
|---|---|---:|
| +Z | nicla-20260925-202324-371157.log | 1.004037 |
| +Z repeat | nicla-20260925-202906-382603.log | 1.004058 |
| +Z repeat | nicla-20260925-203015-159995.log | 1.004012 |
| −Z | nicla-20260925-203251-810834.log | 0.996171 |
| +X | nicla-20260925-204918-341499.log | 0.997509 |
| −X | nicla-20260925-231054-364724.log | 1.001097 |
| +Y | nicla-20260925-230359-245723.log | 1.001040 |
| −Y | nicla-20260925-230543-457844.log | 0.997279 |

User explicitly labeled the supported X/Y recordings. The −Z designation follows
the requested experiment and the measured negative Z dominance.

The four earlier X/Y captures (20:37–20:41) were handheld, as confirmed by the
user. Retain them for comparison, not stationary noise/bias estimation. The
23:01 −X capture contains gyro bursts near 9.81 and 24.68 seconds and is superseded
for stationary characterization by the 23:10 repeat. No original files were
discarded or filtered.

## Findings

- All selected files pass host capture checks, with matching start/end settings,
  no sequence gaps or reported application queue drops, no malformed packets and
  no half-magnitude events. This cannot rule out losses upstream of the callbacks.
- Stationary acceleration magnitude standard deviation is approximately
  0.0007–0.0008 g. Measured delivery rate is near 99.4 Hz, with 100 Hz requested.
- Startup settling is repeatable. Startup data is retained separately; five
  seconds is an analysis boundary, not a universal thermal settling guarantee.
- In the latest −X capture, 2983 samples per stream cover the stationary window.
  Peak gyro magnitude is 0.17795 deg/s versus 3.79806 deg/s in the earlier −X run.
  Gyro mean XYZ is (−0.002363, −0.003264, +0.000358) deg/s.
  Acceleration mean XYZ is approximately (−0.01836, −0.99744, +0.08347) g.
  The larger off-axis Z mean indicates that fixture alignment and residual
  offsets still need separation before precision calibration.
- The observed accelerometer relationship is marked +X → output +Y,
  marked +Y → output −X, marked +Z → output +Z. This is a consistent signed
  axis permutation based on the user's physical labels, not yet a verified
  gyro/quaternion/motorcycle mounting transform.
- Every capture shows the initial accelerometer configuration read failing with
  status −5 and four returned bytes, followed by a successful retry. The library
  calls −5 a timeout; the underlying cause remains unresolved. Retries recover
  these runs but do not constitute a root-cause fix.

## Next experiment

Proceed to controlled gyro-axis rotations, starting with one marked axis.
Record a stationary lead-in, a slow roughly 90-degree rotation about that axis,
a hold, the reverse rotation, and a final hold. Use a guide/hinge to limit
cross-axis motion and record the intended directions. Preserve the current
count units and configuration evidence. This is a dynamic run: do not interpret
the analyzer's 5–35 s means as stationary gyro bias.

Validate axis dominance and sign first. Hardware acquisition timestamps must be
preserved before treating integrated angle error as a rigorous accuracy result.
Precise accelerometer calibration also needs alignment control or a suitably
validated multi-pose method; six approximate face placements are insufficient
evidence for high-accuracy correction constants.

### Controlled rotation results

All three marked axes have now been exercised:

| Marked rotation axis | Log suffix (20260925) | Dominant reported gyro axis |
|---|---|---|
| X | 232248-224341 | Y |
| Y (user-confirmed) | 232656-838633 | X |
| Z (user-confirmed) | 233142-632538 | Z |

These logs are named `nicla-rotation-x-...` because the original capture mode was
reused. The second and third files' automatic experiment labels do not describe
the user-confirmed axis. Individual analysis files preserve this distinction.
Each turn reverses the dominant gyro sign on return. The Z test stays mainly
at +1 g on reported Z, with gyro Z means -15.51 and +15.34 deg/s during outward
and return windows. This agrees with clockwise-first motion when viewed from
above +Z, assuming the requested motion direction was followed.

Combined static/dynamic evidence supports converting reported vector components
to marked-board components as `(reported Y, -reported X, reported Z)`. This is
an axis permutation, not a precision calibration or quaternion transform. There
is no quantitative lean accuracy claim. Hardware timestamps and fusion
output conventions still need validation before dynamic lean estimation.

### Confirmed motorcycle installation

The user confirmed marked +Y forward, marked +Z upward, and marked +X toward
the rider's right. Define motorcycle X forward, Y left, Z up (right-handed).
Then motorcycle components are `(board Y, -board X, board Z)`, giving the combined
mapping `(−reported X, −reported Y, reported Z)` for acceleration and gyro vectors.
`lib/ImuFrames/ImuFrames.h` encodes the two steps separately and composes them.
The native `test_scripts/test_imu_frames.cpp` checks all six face responses,
handedness, magnitude preservation and the signed-count boundary.

Positive rotation about motorcycle X leans the top rightward, consistent with
the app's positive-right/negative-left convention. Reported gyro X is therefore
the primary roll-axis channel for this mounting, with its sign inverted.
This does not mean gyro X alone is a lean angle, or that its integral alone is
accurate during combined roll/pitch/yaw motion.

The component is not yet connected to the diagnostic stream: captured columns
retain their reported coordinates for comparison with prior logs. No firmware
upload is required for this documentation and component step. Small installation
misalignment remains to be measured, and quaternion conventions must be verified
before transforming fused orientation. Never remap quaternion XYZ as a vector.

### Guided X rotation capture

No firmware upload is needed. Close the serial monitor and run the existing
capture tool with `--rotation-x` and the Nicla serial `--port`. It will ask for
a board reset and then Enter to start, as before. It saves a separate
`nicla-rotation-x-*.log` and appends an experiment label after capture.

Start with marked +Z pointing upward and marked X horizontal. Imagine a skewer
running through the enclosure parallel to marked X: rotate around that line.
Use a hinge or stable edge parallel to X to guide the turn, with cable slack.
The intended endpoints are +Z upward, then +Y upward, then +Z upward again.
Follow these terminal prompts (the terminal bell may be muted):

| Firmware elapsed time | Action |
|---|---|
| 0–10 s | Hold the starting position still |
| 10–13 s | Slowly turn approximately 90 degrees about marked X |
| 13–20 s | Hold with +Y upward |
| 20–23 s | Reverse the same motion to the starting position |
| 23–35 s | Hold still until capture completes |

Prompts follow arriving sensor timestamps; PC serial delay and human reaction
make these approximate windows, not measured motion boundaries. Record any
missed prompts or extra movements. This first test establishes axis dominance
and opposite signs, not precise angular accuracy. The static analyzer explicitly
rejects labeled rotation captures to prevent reporting moving samples as bias.
