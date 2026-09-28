"""Check a six-axis quaternion bench capture without assuming its frame convention."""
import argparse
import math
from pathlib import Path
import statistics

MARKER = b"# NICLA_QUATERNION_V1"
HEADER = "sensor_id,sequence,host_us,x_raw,y_raw,z_raw,w_raw,accuracy_raw"
IDS = {4: "accel_events", 13: "gyro_events", 37: "quaternion_events"}


def read_capture(raw):
    start = raw.find(MARKER)
    if start < 0:
        raise ValueError("Missing quaternion marker; upload the new firmware and use --quaternion-test")
    text = raw[start:].decode("utf-8")
    if text.splitlines().count(MARKER.decode()) != 1:
        raise ValueError("Expected exactly one quaternion run")
    rows, configs, end, issues = {sid: [] for sid in IDS}, {}, {}, []
    header_seen = False
    ended = False
    for number, line in enumerate(text.splitlines(), 1):
        line = line.strip()
        if line.startswith("# ERROR"):
            issues.append(line)
        elif line.startswith("# CONFIG,") and not line.startswith("# CONFIG,phase,"):
            try:
                _, phase, sid, rate, latency, full_range, sensitivity = line.split(",")
                key = phase, int(sid)
                if key in configs or phase not in ("start", "end") or key[1] not in IDS:
                    raise ValueError()
                configs[key] = float(rate), int(latency), int(full_range), int(sensitivity)
            except ValueError:
                issues.append(f"Line {number}: malformed/duplicate configuration")
        elif line.startswith("# END,"):
            if ended:
                issues.append("Duplicate END marker")
            ended = True
            try:
                end = {key: int(value) for key, value in
                       (item.split("=", 1) for item in line[6:].split(","))}
            except ValueError:
                issues.append("Malformed END marker")
        elif line == HEADER:
            if header_seen:
                issues.append("Duplicate header")
            header_seen = True
        elif not line or line.startswith("#"):
            continue
        else:
            try:
                sid, seq, us, x, y, z, w, accuracy = map(int, line.split(","))
                if (not header_seen or ended or sid not in IDS or seq < 1 or
                        not 0 <= us < 60_000_000 or not 0 <= accuracy <= 65535 or
                        any(not -32768 <= v <= 32767 for v in (x, y, z, w)) or
                        (sid != 37 and (w != 0 or accuracy != 0))):
                    raise ValueError()
                previous = rows[sid][-1] if rows[sid] else None
                if seq != (previous[0] + 1 if previous else 1):
                    issues.append(f"Sensor {sid}: sequence gap/repetition at {seq}")
                if previous and us < previous[1]:
                    issues.append(f"Sensor {sid}: backwards host timestamp")
                rows[sid].append((seq, us, x, y, z, w, accuracy))
            except ValueError:
                issues.append(f"Line {number}: malformed data")
    if not header_seen or not ended:
        issues.append("Missing header or END marker: incomplete capture")
    for key in (*IDS.values(), "queue_dropped", "malformed", "max_poll_gap_us"):
        if key not in end or end[key] < 0:
            issues.append(f"Missing/invalid END field: {key}")
    for key in ("queue_dropped", "malformed"):
        if end.get(key, 0):
            issues.append(f"Firmware reported {key}={end[key]}")
    for sid, key in IDS.items():
        before, after = configs.get(("start", sid)), configs.get(("end", sid))
        if (before is None or before != after or not math.isfinite(before[0]) or
                abs(before[0] - 50) >= 0.1 or before[1] != 0 or
                (sid != 37 and before[2] != (4 if sid == 4 else 1000))):
            issues.append(f"Sensor {sid}: missing, changed or unexpected configuration")
        if len(rows[sid]) != end.get(key):
            issues.append(f"Sensor {sid}: row/event count mismatch")
        settled = [r for r in rows[sid] if r[1] >= 5_000_000]
        if not 2612 <= len(settled) <= 2888:
            issues.append(f"Sensor {sid}: expected about 2750 post-startup rows; got {len(settled)}")
        if not settled or settled[-1][1] < 59_900_000:
            issues.append(f"Sensor {sid}: data does not reach capture end")
    return rows, issues, text


def quaternion(row):
    """Return XYZW, preserving original sign and norm."""
    return tuple(v / 16384.0 for v in row[2:6])


def report(raw):
    rows, issues, text = read_capture(raw)
    output = ["Six-axis quaternion bench capture (Game Rotation Vector ID 37)"]
    for phase, lo, hi in (("startup", 0, 5_000_000), ("post-startup", 5_000_000, 60_000_000)):
        qrows = [r for r in rows[37] if lo <= r[1] < hi]
        if not qrows:
            continue
        norms = [math.sqrt(sum(v*v for v in quaternion(r))) for r in qrows]
        bad = sum(abs(v - 1.0) > 0.01 for v in norms)
        output.append(f"{phase}: {len(qrows)} quaternion rows; norm min/mean/max "
                      f"{min(norms):.6f}/{statistics.mean(norms):.6f}/{max(norms):.6f}; "
                      f"outside 1 +/- 0.01: {bad}")
        if bad and phase == "post-startup":
            issues.append(f"{bad} post-startup quaternions outside diagnostic norm tolerance")
    for sid in IDS:
        r = [a for a in rows[sid] if a[1] >= 5_000_000]
        if len(r) > 1 and r[-1][1] > r[0][1]:
            rate = (len(r)-1)*1e6/(r[-1][1]-r[0][1])
            gap = max(b[1]-a[1] for a, b in zip(r, r[1:]))
            output.append(f"Sensor {sid}: {rate:.3f} delivered Hz; maximum delivery gap {gap} us")
    flips = sum(sum(x*y for x, y in zip(quaternion(a), quaternion(b))) < 0
                for a, b in zip(rows[37], rows[37][1:]))
    output.append(f"Adjacent negative quaternion dot products: {flips} (q and -q can represent the same pose)")
    output.extend(line for line in text.splitlines()
                  if line.startswith(("# CONFIG_READ_FAILURE", "# EXPERIMENT", "# END")))
    output.append("Checks: " + ("REVIEW REQUIRED" if issues else "passed (host capture and quaternion norm only)"))
    output.extend("- " + issue for issue in issues)
    output.extend([
        "Quaternion XYZW is scaled by 1/16384. Norms were checked before any normalization.",
        "Game RV accuracy=0 is not a confidence estimate. No absolute heading reference is provided.",
        "No lean, Euler angles or mounting transform applied: quaternion direction/frame still needs bench validation.",
        "Use observed motion/hold intervals, not prompt times alone. Host timestamps are not acquisition timestamps.",
    ])
    return "\n".join(output), issues


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    args = parser.parse_args()
    try:
        result, issues = report(args.capture.read_bytes())
    except (OSError, UnicodeError, ValueError) as error:
        parser.exit(2, f"Cannot analyze capture: {error}\n")
    print(result)
    return 2 if issues else 0


if __name__ == "__main__":
    raise SystemExit(main())
