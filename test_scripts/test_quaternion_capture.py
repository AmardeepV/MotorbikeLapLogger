import unittest

from analyze_nicla_quaternion import HEADER, read_capture, report


def capture():
    lines = ["# NICLA_QUATERNION_V1", HEADER]
    for sid, full_range in ((4, 4), (13, 1000), (37, 0)):
        lines.append(f"# CONFIG,start,{sid},50,0,{full_range},0")
    for i in range(1, 3001):
        us = (i-1)*20000
        lines.extend([f"4,{i},{us},0,0,8192,0,0", f"13,{i},{us},0,0,0,0,0",
                      f"37,{i},{us},0,0,0,{16384 if i % 2 else -16384},0"])
    for sid, full_range in ((4, 4), (13, 1000), (37, 0)):
        lines.append(f"# CONFIG,end,{sid},50,0,{full_range},0")
    lines.append("# END,accel_events=3000,gyro_events=3000,quaternion_events=3000,queue_dropped=0,malformed=0,max_poll_gap_us=20000")
    return "\n".join(lines).encode()


class QuaternionCaptureTests(unittest.TestCase):
    def test_q_sign_equivalence_and_binary_boot(self):
        output, issues = report(b"\x80boot\n" + capture())
        self.assertEqual(issues, [])
        self.assertIn("1.000000/1.000000/1.000000", output)
        self.assertIn("dot products: 2999", output)

    def test_invalid_norm_is_not_hidden_by_normalization(self):
        raw = capture().replace(b"37,300,5980000,0,0,0,-16384,0", b"37,300,5980000,0,0,0,0,0")
        self.assertTrue(any("norm" in issue for issue in report(raw)[1]))

    def test_missing_event_detected(self):
        raw = capture().replace(b"37,300,5980000,0,0,0,-16384,0\n", b"")
        self.assertTrue(any("sequence" in issue for issue in read_capture(raw)[1]))

    def test_missing_end_or_bad_range_rejected(self):
        self.assertTrue(read_capture(capture().split(b"# END")[0])[1])
        self.assertTrue(read_capture(capture().replace(b"CONFIG,end,4,50,0,4", b"CONFIG,end,4,50,0,8"))[1])

    def test_legacy_log_not_misinterpreted(self):
        with self.assertRaises(ValueError):
            read_capture(b"# NICLA_DIAGNOSTIC_V1\n")


if __name__ == "__main__":
    unittest.main()
