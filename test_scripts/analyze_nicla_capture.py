"""Analyze one NICLA_DIAGNOSTIC_V1 serial log without discarding outliers.

Only the Python standard library is required. Units use nominal signed-16-bit
full-scale conversion after matching start/end configuration readbacks.
"""
import argparse
import math
from pathlib import Path
import statistics


HEADER = "sensor_id,sequence,host_us,x_counts,y_counts,z_counts"
STARTUP_US = 5_000_000
DURATION_US = 35_000_000
SENSORS = {4: ("Accelerometer", "g", 4), 13: ("Gyroscope", "deg/s", 1000)}


def decode_capture(raw):
    # Reset/boot output can contain non-text bytes. Ignore only the preamble;
    # corrupted bytes within a diagnostic must still fail strict decoding.
    marker = b"# NICLA_DIAGNOSTIC_V1"
    start = raw.find(marker)
    if start < 0:
        raise ValueError("Missing diagnostic marker")
    return raw[start:].decode("utf-8")


def read_capture(text):
    rows = {sensor: [] for sensor in SENSORS}
    configs, end, issues = {}, {}, []
    started = False
    header_seen = False
    ended = False
    for number, raw in enumerate(text.splitlines(), 1):
        line = raw.strip()
        if line == "# NICLA_DIAGNOSTIC_V1":
            if started:
                raise ValueError("Multiple captures: analyze one reset/run per file.")
            started = True
            continue
        if not started:
            continue  # PlatformIO monitor banner, instructions, etc.
        if line.startswith("# ERROR"):
            issues.append(line)
        elif line.startswith("# CONFIG,") and not line.startswith("# CONFIG,phase,"):
            try:
                _, phase, sid, rate, latency, full_range, sensitivity = line.split(",")
                sid = int(sid)
                config = (float(rate), int(latency), int(full_range), int(sensitivity))
                if phase not in ("start", "end") or sid not in SENSORS:
                    raise ValueError()
                if (phase, sid) in configs:
                    issues.append(f"Line {number}: duplicate configuration")
                configs[phase, sid] = config
            except ValueError:
                issues.append(f"Line {number}: invalid configuration")
        elif line.startswith("# END,"):
            if ended:
                issues.append(f"Line {number}: duplicate END marker")
            ended = True
            try:
                end = dict(item.split("=", 1) for item in line[6:].split(","))
                end = {key: int(value) for key, value in end.items()}
            except ValueError:
                issues.append(f"Line {number}: invalid END marker")
        elif line == HEADER:
            if header_seen:
                issues.append(f"Line {number}: duplicate data header")
            header_seen = True
        elif not line or line.startswith("#"):
            continue
        else:
            try:
                sid, sequence, host_us, x, y, z = map(int, line.split(","))
                if (not header_seen or ended or sid not in SENSORS or sequence < 1 or
                        not 0 <= host_us < DURATION_US or
                        any(not -32768 <= v <= 32767 for v in (x, y, z))):
                    raise ValueError()
                previous = rows[sid][-1] if rows[sid] else None
                expected = previous[0] + 1 if previous else 1
                if sequence != expected:
                    issues.append(f"Line {number}: sensor {sid} sequence {sequence}, expected {expected}")
                if previous and host_us < previous[1]:
                    issues.append(f"Line {number}: host time moved backwards")
                rows[sid].append((sequence, host_us, x, y, z))
            except ValueError:
                issues.append(f"Line {number}: malformed data row: {line[:100]}")

    if not started or not header_seen:
        issues.append("Missing diagnostic marker or data header")
    if not ended:
        issues.append("Missing END marker: capture incomplete")
    for key in ("accel_events", "gyro_events", "queue_dropped", "malformed", "max_poll_gap_us"):
        if key not in end or end[key] < 0:
            issues.append(f"Missing/invalid END field: {key}")
    for key in ("queue_dropped", "malformed"):
        if end.get(key, 0):
            issues.append(f"Firmware reported {key}={end[key]}")
    for sid, (_, _, expected_range) in SENSORS.items():
        before, after = configs.get(("start", sid)), configs.get(("end", sid))
        if (before is None or after is None or before != after or
                not math.isfinite(before[0]) or abs(before[0] - 100) >= 0.1 or
                before[1] != 0 or before[2] != expected_range):
            issues.append(f"Sensor {sid}: start/end configuration missing, changed, or unexpected")
        count_key = "accel_events" if sid == 4 else "gyro_events"
        if end.get(count_key) != len(rows[sid]):
            issues.append(f"Sensor {sid}: saved row count differs from firmware event count")
        steady = [row for row in rows[sid] if row[1] >= STARTUP_US]
        # A diagnostic tolerance, not a sensor accuracy specification.
        if not 2850 <= len(steady) <= 3150:
            issues.append(f"Sensor {sid}: {len(steady)} steady rows; expected about 3000 (+/-5%)")
        if not steady or steady[-1][1] < DURATION_US - 100_000:
            issues.append(f"Sensor {sid}: data does not cover the end of the capture")
    return rows, configs, end, issues


def describe(values):
    return (f"mean={statistics.mean(values):.6f}, min={min(values):.6f}, "
            f"max={max(values):.6f}, population_sd={statistics.pstdev(values):.6f}")


def report(text):
    if "# EXPERIMENT,rotation-x" in text:
        raise ValueError("This is a controlled rotation capture, not a stationary test. "
                         "Analyze the turn/hold/return phases separately; do not estimate bias from the entire run.")
    rows, configs, end, issues = read_capture(text)
    output = ["Nicla stationary sensor diagnostic", ""]
    output.append("Capture checks: " + ("REVIEW REQUIRED" if issues else "passed (host capture checks only)"))
    output.extend("- " + issue for issue in issues)
    output.append(f"Maximum host polling gap: {end.get('max_poll_gap_us', 'unknown')} us")
    output.append("Timing is host delivery timing, not sensor acquisition timing.")
    output.append("A clean capture cannot rule out losses upstream of the Arduino callbacks.")
    read_failures = [line.strip() for line in text.splitlines()
                     if line.strip().startswith("# CONFIG_READ_FAILURE,")]
    if read_failures:
        output.append(f"Configuration read failures (retained even if a retry succeeded): {len(read_failures)}")
        output.extend(read_failures)

    for sid, (name, unit, _) in SENSORS.items():
        output.extend(["", f"{name} (sensor {sid}, Bosch-corrected output)"])
        before, after = configs.get(("start", sid)), configs.get(("end", sid))
        verified = (before is not None and before == after and
                    before[2] == SENSORS[sid][2] and math.isfinite(before[0]) and
                    abs(before[0] - 100) < 0.1 and before[1] == 0 and
                    not any(issue.startswith("# ERROR") for issue in issues))
        for phase, subset in (
                ("startup (0-5 s)", [row for row in rows[sid] if row[1] < STARTUP_US]),
                ("stationary window (5-35 s)", [row for row in rows[sid] if row[1] >= STARTUP_US])):
            output.append(f"  {phase}: {len(subset)} rows")
            if not subset:
                continue
            for axis, index in zip("XYZ", (2, 3, 4)):
                output.append(f"    {axis} counts: {describe([row[index] for row in subset])}")
            magnitudes = [math.sqrt(sum(value * value for value in row[2:])) for row in subset]
            output.append("    magnitude counts: " + describe(magnitudes))
            clipped = sum(any(v in (-32768, 32767) for v in row[2:]) for row in subset)
            output.append(f"    rows at signed-16-bit rails: {clipped} (not a complete saturation detector)")
            if len(subset) > 1:
                intervals = [b[1] - a[1] for a, b in zip(subset, subset[1:])]
                span = subset[-1][1] - subset[0][1]
                rate = (len(subset) - 1) * 1e6 / span if span else float('nan')
                output.append(f"    mean delivered rate: {rate:.3f} Hz; "
                              f"median/max delivery gap: {statistics.median(intervals):.0f}/{max(intervals)} us")
            if verified and phase.startswith("stationary"):
                factor = before[2] / 32768.0
                output.append(f"    nominal conversion: count * {factor:.9f} {unit}")
                if sid == 4:
                    output.append("    magnitude g: " + describe([v * factor for v in magnitudes]))
                else:
                    means = [statistics.mean(row[i] for row in subset) * factor for i in (2, 3, 4)]
                    output.append("    residual gyro mean XYZ (deg/s): " + ", ".join(f"{v:.6f}" for v in means))
                    output.append("    Interpret as residual bias only if the enclosure was stationary; no correction applied.")
        if sid == 4 and rows[sid]:
            # Relative to the steady median: detect the reported half-magnitude
            # pattern without assuming gravity is already calibrated to 1 g.
            steady_norms = [math.sqrt(sum(v * v for v in row[2:]))
                            for row in rows[sid] if row[1] >= STARTUP_US]
            if steady_norms and statistics.median(steady_norms) > 0:
                reference = statistics.median(steady_norms)
                low = [row for row in rows[sid]
                       if math.sqrt(sum(v * v for v in row[2:])) < 0.75 * reference]
                output.append(f"  magnitude below 75% of steady median: {len(low)} rows "
                              f"({sum(row[1] < STARTUP_US for row in low)} during startup)")
                output.append("  first low-magnitude (sequence, host seconds): " +
                              ", ".join(f"({row[0]}, {row[1] / 1e6:.6f})" for row in low[:20]))
                output.append("  This threshold is an investigation aid; no rows were removed.")
    return "\n".join(output), issues


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path, help="One serial log with # metadata preserved")
    args = parser.parse_args()
    try:
        result, issues = report(decode_capture(args.capture.read_bytes()))
    except (OSError, UnicodeError, ValueError) as error:
        parser.exit(2, f"Cannot analyze capture: {error}\n")
    print(result)
    return 2 if issues else 0


if __name__ == "__main__":
    raise SystemExit(main())
