"""Save one Nicla diagnostic run verbatim, including configuration and errors.

Run with PlatformIO's Python (which already includes pyserial), or install
pyserial in your own Python environment. Close other serial monitors first.
"""
from contextlib import nullcontext
import argparse
import math
from datetime import datetime
from pathlib import Path
import time


ROTATION_X_CUES = (
    (0, "HOLD: +Z upward; marked X axis horizontal. Keep still."),
    (10_000_000, "TURN: slowly rotate about marked X until +Y points upward (about 90 degrees, over 3 seconds)."),
    (13_000_000, "HOLD: keep the new position still."),
    (20_000_000, "RETURN: rotate back along the same path until +Z points upward (over 3 seconds)."),
    (23_000_000, "HOLD: keep still until recording finishes."),
)

QUATERNION_CUES = {
    "upright": ((0, "HOLD: keep the box upright and motionless for the full minute."),),
    "roll": (
        (0, "HOLD UPRIGHT: marked +Z up, +Y pointing away from you (motorcycle forward)."),
        (10_000_000, "LEAN RIGHT: tilt the top toward marked +X by about 30 degrees over 5 seconds; pivot about marked Y."),
        (15_000_000, "HOLD RIGHT: maintain that tilt."),
        (22_000_000, "RETURN UPRIGHT: slowly bring +Z vertical over 5 seconds."),
        (27_000_000, "HOLD UPRIGHT."),
        (32_000_000, "LEAN LEFT: tilt the top toward marked -X by about 30 degrees over 5 seconds; pivot about marked Y."),
        (37_000_000, "HOLD LEFT: maintain that tilt."),
        (44_000_000, "RETURN UPRIGHT: slowly bring +Z vertical over 5 seconds."),
        (49_000_000, "HOLD UPRIGHT until recording finishes."),
    ),
    "pitch": (
        (0, "HOLD UPRIGHT: marked +Z up, +Y pointing away from you (motorcycle forward)."),
        (10_000_000, "NOSE UP: raise the +Y/front end about 20 degrees over 5 seconds; pivot about marked X."),
        (15_000_000, "HOLD NOSE UP."),
        (22_000_000, "RETURN LEVEL over 5 seconds."),
        (27_000_000, "HOLD LEVEL."),
        (32_000_000, "NOSE DOWN: lower the +Y/front end about 20 degrees over 5 seconds; pivot about marked X."),
        (37_000_000, "HOLD NOSE DOWN."),
        (44_000_000, "RETURN LEVEL over 5 seconds."),
        (49_000_000, "HOLD LEVEL until recording finishes."),
    ),
}


# Times relative to the first valid calibrated reading, not firmware boot.
LEAN_CUES = (
    (0, "CALIBRATED: keep upright for 5 seconds. Right lean is positive; nose-up pitch is positive."),
    (5_000_000, "LEAN RIGHT about 25 degrees over 5 seconds, then hold."),
    (15_000_000, "KEEP RIGHT LEAN: raise the front about 15 degrees over 5 seconds, then hold."),
    (25_000_000, "KEEP RIGHT LEAN: return the front to level over 5 seconds, then hold."),
    (35_000_000, "KEEP RIGHT LEAN: lower the front about 15 degrees over 5 seconds, then hold."),
    (45_000_000, "RETURN UPRIGHT AND LEVEL over 5 seconds, then hold."),
    (55_000_000, "LEAN LEFT about 25 degrees over 5 seconds, then hold."),
    (65_000_000, "KEEP LEFT LEAN: raise the front about 15 degrees over 5 seconds, then hold."),
    (75_000_000, "KEEP LEFT LEAN: return the front to level over 5 seconds, then hold."),
    (85_000_000, "KEEP LEFT LEAN: lower the front about 15 degrees over 5 seconds, then hold."),
    (95_000_000, "RETURN UPRIGHT AND LEVEL over 5 seconds; leave still until finished."),
)


MOTION_CUES = (
    (0, "CALIBRATED: keep upright and still for 15 seconds; acceleration should be near zero."),
    (15_000_000, "SLIDE: keep the box level; gently move it forward (+Y) and stop, then return. Repeat slowly for 15 seconds; leave cable slack."),
    (30_000_000, "REST: leave the box upright and still for 15 seconds."),
    (45_000_000, "NOSE UP: raise the front about 15 degrees over 5 seconds, then hold still."),
    (60_000_000, "RETURN LEVEL over 5 seconds, then hold still."),
    (75_000_000, "LEAN: tilt about 25 degrees over 5 seconds, then hold still."),
    (90_000_000, "RETURN UPRIGHT over 5 seconds; remain still until finished."),
)


def save_stream(connection, output, timeout=90, rotation_x=False, quaternion_test=None, lean_test=False, motion_test=False):
    """Keep raw bytes, even partial lines; stop at END, ERROR or a deadline."""
    lean_test = lean_test or motion_test
    pending = b""
    started = False
    deadline = time.monotonic() + timeout
    next_progress = time.monotonic() + 5
    total = 0
    cue_index = 0
    calibrated_us = None
    last_lean_us = None
    lean_status = None
    last_lean_wall = None
    latest_lean_us = None
    last_motion_display_us = None
    motion_rows = 0
    previous_motion_us = None
    last_motion_wall = None
    previous_motion_status = None
    cues = MOTION_CUES if motion_test else LEAN_CUES if lean_test else QUATERNION_CUES[quaternion_test] if quaternion_test else ROTATION_X_CUES if rotation_x else ()
    expected_marker = b"# NICLA_MOTION_V1" if motion_test else b"# NICLA_LEAN_V1" if lean_test else b"# NICLA_QUATERNION_V1" if quaternion_test else b"# NICLA_DIAGNOSTIC_V1"
    while time.monotonic() < deadline:
        chunk = connection.read(min(connection.in_waiting or 1, 4096))
        if chunk:
            output.write(chunk)
            output.flush()
            total += len(chunk)
            pending += chunk
            while b"\n" in pending:
                line, pending = pending.split(b"\n", 1)
                line = line.strip()
                if line in (b"# NICLA_DIAGNOSTIC_V1", b"# NICLA_QUATERNION_V1", b"# NICLA_LEAN_V1", b"# NICLA_MOTION_V1"):
                    if line != expected_marker:
                        return "wrong firmware capture mode; reset and check the upload/command"
                    if started:
                        return "multiple runs detected; file retained for review"
                    started = True
                if started and line.startswith(b"# ERROR"):
                    return "firmware reported an error; file retained for review"
                if started and line.startswith(b"# END,"):
                    if lean_test and (calibrated_us is None or latest_lean_us-calibrated_us < 105_000_000):
                        return "recorded, but calibration or guided sequence did not finish; file retained"
                    if motion_test and not motion_rows:
                        return "missing motion records; file retained"
                    return "complete"
                if started and motion_test and line.startswith(b"M,"):
                    try:
                        from analyze_nicla_motion import parse_motion
                        m = parse_motion(line.decode("ascii"))
                    except (ValueError, UnicodeError):
                        return "malformed motion row; file retained"
                    if m[0] != motion_rows+1 or (previous_motion_us is not None and m[1]<=previous_motion_us):
                        return "motion sequence/timestamp discontinuity; file retained"
                    motion_rows += 1
                    previous_motion_us, last_motion_wall = m[1], time.monotonic()
                    if m[2] != previous_motion_status or last_motion_display_us is None or m[1]-last_motion_display_us>=500_000:
                        if m[2] == "VALID":
                            print(f"Estimated forward acceleration {m[3]:+.3f} m/s² | left {m[4]:+.3f} | body-up {m[5]:+.3f}", flush=True)
                        else:
                            print(f"Motion {m[2]}: acceleration and angular rates unavailable.", flush=True)
                        last_motion_display_us=m[1]
                        previous_motion_status=m[2]
                if started and lean_test and line.startswith(b"L,"):
                    fields = line.decode("ascii", errors="replace").split(",")
                    if len(fields) != 6:
                        return "malformed lean display row; file retained"
                    try:
                        host_us, calibration = int(fields[1]), int(fields[5])
                        status = fields[2]
                        if status not in ("VALID", "STARTUP", "CALIBRATING", "STALE", "INVALID", "CAPTURE_FAULT"):
                            raise ValueError()
                        if host_us < 0 or calibration < 0 or (latest_lean_us is not None and host_us <= latest_lean_us):
                            raise ValueError()
                        if status == "VALID":
                            roll, pitch = float(fields[3]), float(fields[4])
                            if not (math.isfinite(roll) and math.isfinite(pitch) and abs(roll)<=180 and abs(pitch)<=90 and calibration>0):
                                raise ValueError()
                        elif fields[3] or fields[4]:
                            raise ValueError()
                    except ValueError:
                        return "malformed lean display row; file retained"
                    latest_lean_us, last_lean_wall = host_us, time.monotonic()
                    if status != lean_status or last_lean_us is None or host_us-last_lean_us >= 500_000:
                        if status == "VALID":
                            print(f"Lean {fields[3]} degrees | Pitch {fields[4]} degrees", flush=True)
                        else:
                            print(f"{status}: angles unavailable; keep upright and still during calibration.", flush=True)
                        last_lean_us, lean_status = host_us, status
                    if status == "CAPTURE_FAULT":
                        return "capture fault; reset required; file retained"
                    if status == "VALID" and calibrated_us is None:
                        calibrated_us = host_us
                    if calibrated_us is not None and status != "VALID":
                        return "lean became unavailable during the guided test; file retained, reset before retrying"
                    if calibrated_us is not None:
                        while cue_index < len(cues) and host_us-calibrated_us >= cues[cue_index][0]:
                            print("\a" + cues[cue_index][1], flush=True)
                            cue_index += 1
                if started and not lean_test and cues and line.startswith((b"4,", b"13,", b"37,")):
                    fields = line.split(b",")
                    if len(fields) != (8 if quaternion_test else 6):
                        continue
                    try:
                        host_us = int(fields[2])
                    except ValueError:
                        continue
                    # Follow the firmware's elapsed clock, not PC startup time.
                    # Human reaction and serial delay make these approximate cues.
                    while cue_index < len(cues) and host_us >= cues[cue_index][0]:
                        print("\a" + cues[cue_index][1], flush=True)
                        cue_index += 1
            if len(pending) > 65536:
                return "unexpected serial data without line endings; file retained"
        if motion_test and last_motion_wall is not None and time.monotonic()-last_motion_wall > 1.0:
            print("STALE: motion stream stopped; acceleration and angular rates unavailable.", flush=True)
            return "motion stream stopped; incomplete file retained"
        if lean_test and last_lean_wall is not None and time.monotonic()-last_lean_wall > 1.0:
            print("STALE: no live update received; angles unavailable.", flush=True)
            return "live stream stopped; incomplete file retained"
        if time.monotonic() >= next_progress:
            print(f"Live... {total:,} bytes received" if isinstance(output, DiscardOutput) else f"Recording... {total:,} bytes saved", flush=True)
            next_progress = time.monotonic() + 5
    return "timed out; incomplete file retained (reset the board before retrying)"


class DiscardOutput:
    """Live-only sink: never opens a file or retains received chunks."""
    def write(self, chunk):
        return len(chunk)

    def flush(self):
        pass


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Serial device; automatically selected if exactly one exists")
    parser.add_argument("--list", action="store_true", help="List available serial devices")
    parser.add_argument("--output", type=Path, help="New log file; existing files are never overwritten")
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument("--rotation-x", action="store_true", help="Guide an X-axis quarter-turn and return instead of a stationary run")
    modes.add_argument("--quaternion-test", choices=tuple(QUATERNION_CUES),
                       help="60 s Game Rotation Vector capture; requires updated firmware")
    modes.add_argument("--lean-test", action="store_true", help="120 s live lean/pitch test with upright calibration and raw recording")
    modes.add_argument("--motion-test", action="store_true", help="120 s acceleration, angular-rate and magnetometer recording")
    modes.add_argument("--live-motion", action="store_true", help="120 s live motion test; no file or session recording")
    args = parser.parse_args()
    if args.live_motion and args.output:
        parser.error("--live-motion does not save files; do not specify --output")
    args.motion_test = args.motion_test or args.live_motion
    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        parser.exit(2, "pyserial is missing. Run this tool with ~/.platformio/penv/bin/python.\n")

    ports = list(list_ports.comports())
    if args.list or (not args.port and len(ports) != 1):
        for port in ports:
            print(f"{port.device}  {port.description}")
        if not ports:
            print("No serial devices found. Connect the Nicla by USB.")
        if args.list:
            return 0
        parser.exit(2, "Run again with --port followed by the Nicla's device path.\n")
    port = args.port or ports[0].device
    experiment = "motion" if args.motion_test else "lean" if args.lean_test else f"quaternion-{args.quaternion_test}" if args.quaternion_test else "rotation-x" if args.rotation_x else None
    destination = args.output or (
        Path(__file__).resolve().parents[1] / "captures" /
        f"nicla-{experiment + '-' if experiment else ''}{datetime.now():%Y%m%d-%H%M%S-%f}.log")
    destination = destination.expanduser().resolve()
    created = False
    result = "incomplete"
    try:
        if not args.live_motion:
            destination.parent.mkdir(parents=True, exist_ok=True)
        with (nullcontext(DiscardOutput()) if args.live_motion else destination.open("xb")) as output:
            created = not args.live_motion
            with serial.Serial(port, 115200, timeout=0.2, write_timeout=2) as connection:
                print(f"Connected to {port}. Live display only; no recording." if args.live_motion
                      else f"Connected to {port}. Saving to:\n{destination}")
                print("Reset the Nicla now, then place the enclosure motionless with the cable slack.")
                if args.lean_test or args.motion_test:
                    print("Start truly upright and level: +Z up, +Y forward. Support it motionless for calibration.")
                    print("After calibration, follow the movement prompts. Hand movement limits accuracy conclusions.")
                if args.rotation_x:
                    print("Start with +Z upward and marked X horizontal. Guide the motion about X only.")
                    print("Follow TURN / HOLD / RETURN prompts; do not spin around vertical Z.")
                if args.quaternion_test:
                    print(QUATERNION_CUES[args.quaternion_test][0][1])
                    print("Use a support to hold each pose. Follow the prompts; no full 90-degree turns needed.")
                input("When the board has restarted and the box is still, press Enter to begin: ")
                # Capture starts only after the user has reset and positioned it.
                # Do not clear the input buffer: retain any diagnostic evidence.
                connection.write(b"m" if args.motion_test else b"l" if args.lean_test else b"q" if args.quaternion_test else b"s")
                connection.flush()
                print(("Live test for about " if args.live_motion else "Recording for about ") + f"{120 if args.lean_test or args.motion_test else 60 if args.quaternion_test else 35} seconds. " +
                      ("Follow the prompts." if experiment else "Keep the enclosure still."))
                result = save_stream(connection, output, timeout=150 if args.lean_test or args.motion_test else 90,
                                     rotation_x=args.rotation_x, quaternion_test=args.quaternion_test,
                                     lean_test=args.lean_test, motion_test=args.motion_test)
    except (KeyboardInterrupt, EOFError):
        result = "cancelled; incomplete file retained"
    except (OSError, serial.SerialException) as error:
        result = f"capture failed: {error}"
    if created and experiment:
        # Append only after closing the serial stream. Inserting comments between
        # arbitrary serial chunks could split a sensor row and corrupt the log.
        try:
            with destination.open("ab") as output:
                output.write(f"\n# EXPERIMENT,{experiment}\n".encode())
                if args.rotation_x:
                    output.write(b"# planned_turn_us=10000000:13000000,planned_return_us=20000000:23000000\n")
                elif args.lean_test or args.motion_test:
                    output.write(b"# cues_relative_to_first_VALID_reading_us\n")
                    for at, cue in (MOTION_CUES if args.motion_test else LEAN_CUES):
                        output.write(f"# PLANNED_CUE,{at},{cue}\n".encode())
                else:
                    for at, cue in QUATERNION_CUES[args.quaternion_test]:
                        output.write(f"# PLANNED_CUE,{at},{cue}\n".encode())
        except OSError as error:
            result = f"could not label rotation capture: {error}"
    if created and args.motion_test and result == "complete":
        try:
            from analyze_nicla_motion import analyze
            summary, issues = analyze(destination.read_bytes())
            print(summary)
            if issues:
                result = "recorded; motion validation needs review"
        except (OSError, UnicodeError, ValueError) as error:
            result = f"recorded; motion validation failed: {error}"
    if args.live_motion:
        result = result.replace("incomplete file retained", "no recording saved").replace("file retained", "no recording saved").replace("bytes saved", "bytes received")
        print(f"Live test {result}. No data file was created.")
    else:
        print(f"Capture {result}.")
    if created:
        print(f"File: {destination}")
        print("Share this entire file, including when capture failed.")
    return 0 if result == "complete" else 2


if __name__ == "__main__":
    raise SystemExit(main())
