"""Validate a NICLA_MOTION_V1 recording; summarize experimental body acceleration."""
import argparse
import math
from pathlib import Path
import statistics
from collections import Counter

MARKER = b"# NICLA_MOTION_V1"
STATUSES = {"STARTUP", "CALIBRATING", "STALE", "INVALID", "CAPTURE_FAULT", "VALID"}
SENSORS = {4: (50, "accel_events"), 13: (50, "gyro_events"),
           22: (12.5, "magnetometer_events"), 37: (50, "quaternion_events")}


def parse_motion(line):
    fields = line.strip().split(",")
    if len(fields) != 15 or fields[0] != "M":
        raise ValueError("Expected 15-field motion record")
    seq, us, status = int(fields[1]), int(fields[2]), fields[3]
    ages = tuple(map(int, fields[10:13]))
    flags, calibration = int(fields[13]), int(fields[14])
    if seq < 1 or not 0 <= us <= 120_100_000 or status not in STATUSES or not 0 <= flags <= 7 or calibration < 0 or any(a < 0 for a in ages):
        raise ValueError("Invalid motion metadata")
    if status == "VALID":
        values = tuple(map(float, fields[4:10]))
        if (not all(math.isfinite(v) for v in values) or calibration == 0 or
                max(ages) > 100_000 or max(ages)-min(ages) > 40_000 or flags & 5):
            raise ValueError("Invalid usable motion record")
    else:
        if any(fields[4:10]):
            raise ValueError("Unavailable motion must have empty value fields")
        values = (math.nan,) * 6
    return (seq, us, status, *values, *ages, flags, calibration)


def analyze(raw):
    start = raw.find(MARKER)
    if start < 0:
        raise ValueError("Missing motion marker; use updated firmware and --motion-test")
    text = raw[start:].decode("utf-8")
    issues, motion, lean = [], [], []
    rows = {sid: [] for sid in SENSORS}
    configs, end = {}, {}
    ended = False
    header = "sensor_id,sequence,host_us,x_raw,y_raw,z_raw,w_raw,accuracy_raw"
    if text.splitlines().count(header) != 1:
        issues.append("Expected exactly one raw data header")
    if text.splitlines().count(MARKER.decode()) != 1:
        issues.append("Expected one motion run")
    for number, line in enumerate(text.splitlines(), 1):
        if not line:
            continue
        f = line.split(",")
        try:
            if line.startswith("# ERROR"):
                issues.append(line)
            elif f[0] == "# CONFIG" and f[1] != "phase":
                if len(f) != 7 or f[1] not in ("start", "end") or int(f[2]) not in SENSORS:
                    raise ValueError("Invalid configuration")
                key = f[1], int(f[2])
                if key in configs:
                    raise ValueError("Duplicate configuration")
                configs[key] = (float(f[3]), *map(int, f[4:]))
            elif f[0] == "# END":
                if ended:
                    raise ValueError("Duplicate END")
                ended = True
                end = {k: int(v) for k, v in (p.split("=") for p in f[1:])}
            elif f[0] == "M":
                if ended:
                    raise ValueError("Motion after END")
                m = parse_motion(line)
                if m[0] != len(motion)+1 or (motion and m[1] <= motion[-1][1]):
                    issues.append("Motion sequence/timestamp discontinuity")
                motion.append(m)
            elif f[0] == "L":
                if ended or len(f) != 6 or f[2] not in STATUSES:
                    raise ValueError("Invalid lean row")
                t, cal = int(f[1]), int(f[5])
                if not 0 <= t <= 120_100_000 or cal < 0 or (lean and t <= lean[-1][0]):
                    raise ValueError("Invalid lean metadata")
                if f[2] == "VALID":
                    a, b = float(f[3]), float(f[4])
                    if not math.isfinite(a) or not math.isfinite(b) or abs(a)>180 or abs(b)>90 or cal==0:
                        raise ValueError("Invalid lean angles")
                elif f[3] or f[4]:
                    raise ValueError("Unavailable lean must have blank angles")
                lean.append((t, f[2], cal))
            elif f[0].isdigit():
                sid, seq, us, x, y, z, w, accuracy = map(int, f)
                if (ended or sid not in rows or not 0 <= us < 120_000_000 or
                        any(not -32768 <= v <= 32767 for v in (x,y,z,w)) or
                        not 0 <= accuracy <= 65535 or (sid != 37 and (w or accuracy))):
                    raise ValueError("Invalid raw row")
                r = rows[sid]
                if seq != len(r)+1 or (r and us < r[-1][1]):
                    issues.append(f"Sensor {sid} sequence/timestamp discontinuity")
                if sid == 37 and not 0.99**2 <= sum(v*v for v in (x,y,z,w))/16384**2 <= 1.01**2:
                    issues.append("Quaternion norm outside tolerance")
                r.append((seq, us))
            elif not line.startswith(("#", "sensor_id,")):
                raise ValueError("Unknown record")
        except (ValueError, IndexError) as error:
            issues.append(f"Line {number}: {error}")
    if not ended:
        issues.append("Missing END: incomplete capture")
    for key in (*[v[1] for v in SENSORS.values()], "queue_dropped", "malformed", "max_poll_gap_us"):
        if key not in end or end[key] < 0:
            issues.append(f"Missing/invalid END field {key}")
    if end.get("queue_dropped", 0) or end.get("malformed", 0):
        issues.append("Firmware reported queue drops or malformed packets")
    output = ["Motion capture: experimental acceleration, body rates and raw magnetic counts",
              f"Ignored {start} pre-marker bytes."]
    for sid, (rate, key) in SENSORS.items():
        before, after = configs.get(("start", sid)), configs.get(("end", sid))
        if (not before or before != after or not math.isfinite(before[0]) or
                abs(before[0]-rate) > 0.1 or before[1] != 0 or
                (sid in (4,13) and before[2] != (4 if sid==4 else 1000))):
            issues.append(f"Sensor {sid}: missing/changed/unexpected configuration")
        r = rows[sid]
        if len(r) != end.get(key) or not 0.95*rate*115 <= sum(t>=5_000_000 for _,t in r) <= 1.05*rate*115 or not r or r[-1][1] < 119_800_000:
            issues.append(f"Sensor {sid}: count/coverage mismatch")
        output.append(f"Sensor {sid}: {len(r)} raw rows")
    if not motion or motion[-1][1] < 119_800_000 or len(motion) < 1100:
        issues.append("Missing/incomplete motion output")
    if [m[1] for m in motion] != [r[0] for r in lean]:
        issues.append("Lean/motion output timestamps do not pair")
    valid = [m for m in motion if m[2] == "VALID"]
    first = next((r[0] for r in lean if r[1] == "VALID"), None)
    if not valid or first is None:
        issues.append("Calibration never produced usable motion")
    output.append(f"Motion statuses: {dict(Counter(m[2] for m in motion))}")
    output.append(f"Flag counts: {dict(Counter(m[12] for m in motion))}")
    if first is not None:
        output.append(f"First valid lean: {first/1e6:.3f} s")
        for name, lo, hi in (("initial rest",2,12),("rest after sliding",34,43),("nose-up hold",54,59),("lean hold",84,89),("final rest",100,105)):
            window = [m for m in valid if lo*1e6 <= m[1]-first < hi*1e6]
            if window:
                means = [statistics.mean(m[i] for m in window) for i in (3,4,5)]
                output.append(f"{name}: n={len(window)}, forward/left/up means = " +
                              "/".join(f"{v:+.4f}" for v in means) + " m/s² (planned window; verify observed motion)")
    output.extend(l for l in text.splitlines() if l.startswith(("# CONFIG_READ_FAILURE", "# END")))
    output.append("Checks: " + ("REVIEW REQUIRED" if issues else "passed (format, continuity, configuration and coverage only)"))
    output.extend("- " + i for i in dict.fromkeys(issues))
    output.append("Body-frame linear acceleration depends on the estimated gravity direction. VALID does not mean independently accurate during cornering. Magnetometer units/frame are not validated. Host times are delivery times.")
    return "\n".join(output), issues


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    args = parser.parse_args()
    try:
        text, issues = analyze(args.capture.read_bytes())
    except (OSError, UnicodeError, ValueError) as error:
        parser.exit(2, str(error)+"\n")
    print(text)
    return 2 if issues else 0


if __name__ == "__main__":
    raise SystemExit(main())
