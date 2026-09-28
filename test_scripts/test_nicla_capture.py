import unittest

from analyze_nicla_capture import HEADER, decode_capture, read_capture, report


def capture():
    lines = ["# NICLA_DIAGNOSTIC_V1", HEADER,
             "# CONFIG,start,4,100.000,0,4,8192",
             "# CONFIG,start,13,100.000,0,1000,32"]
    for sequence in range(1, 3501):
        timestamp = (sequence - 1) * 10000
        # Deliberate half-magnitude startup sample, with every subsequent
        # sample identical. Identical values must not be deduplicated.
        z = 4096 if sequence == 1 else 8192
        lines += [f"4,{sequence},{timestamp},0,0,{z}",
                  f"13,{sequence},{timestamp},16,-32,0"]
    lines += ["# CONFIG,end,4,100.000,0,4,8192",
              "# CONFIG,end,13,100.000,0,1000,32",
              "# END,accel_events=3500,gyro_events=3500,queue_dropped=0,malformed=0,max_poll_gap_us=10000"]
    return "\n".join(lines)


class CaptureTests(unittest.TestCase):
    def test_dynamic_run_cannot_be_reported_as_stationary_bias(self):
        with self.assertRaisesRegex(ValueError, "controlled rotation"):
            report(capture() + "\n# EXPERIMENT,rotation-x\n")

    def test_binary_boot_preamble_is_ignored(self):
        text = capture()
        self.assertEqual(decode_capture(b"Booting\r\n\x80\xff" + text.encode()), text)

    def test_binary_corruption_inside_capture_is_rejected(self):
        with self.assertRaises(UnicodeDecodeError):
            decode_capture(capture().encode() + b"\x80")

    def test_preserves_identical_samples_and_startup_outlier(self):
        rows, _, _, issues = read_capture(capture())
        self.assertEqual(issues, [])
        self.assertEqual(len(rows[4]), 3500)
        output, _ = report(capture())
        self.assertIn("1 rows (1 during startup)", output)
        self.assertIn("magnitude g: mean=1.000000", output)
        self.assertIn("0.488281, -0.976562, 0.000000", output)

    def test_missing_record_detected(self):
        text = capture().replace("4,200,1990000,0,0,8192\n", "")
        issues = read_capture(text)[3]
        self.assertTrue(any("sequence" in issue for issue in issues))
        self.assertTrue(any("row count" in issue for issue in issues))

    def test_truncation_and_overflow_detected(self):
        self.assertTrue(read_capture(capture().split("# END")[0])[3])
        issues = read_capture(capture().replace("queue_dropped=0", "queue_dropped=2"))[3]
        self.assertTrue(any("queue_dropped=2" in issue for issue in issues))

    def test_unverified_ranges_prevent_conversion(self):
        output, issues = report(capture().replace("# CONFIG,end,4,100.000,0,4,8192",
                                                "# CONFIG,end,4,100.000,0,8,4096"))
        self.assertTrue(issues)
        self.assertNotIn("magnitude g:", output)

    def test_invalid_counts_rejected(self):
        issues = read_capture(capture().replace("0,0,4096", "0,0,40000"))[3]
        self.assertTrue(any("malformed data" in issue for issue in issues))

    def test_several_samples_can_share_delivery_time(self):
        self.assertFalse(read_capture(capture().replace("4,2,10000,", "4,2,0,"))[3])

    def test_multiple_runs_rejected(self):
        with self.assertRaisesRegex(ValueError, "Multiple captures"):
            read_capture(capture() + "\n" + capture())

    def test_recovered_read_failure_remains_visible(self):
        failure = "# CONFIG_READ_FAILURE,phase=start,sensor_id=4,attempt=1,status=-8,bytes=0,expected_bytes=12"
        output, issues = report(capture().replace(HEADER, HEADER + "\n" + failure))
        self.assertEqual(issues, [])
        self.assertIn(failure, output)

    def test_exhausted_read_failure_prevents_conversion(self):
        output, issues = report(capture() + "\n# ERROR,configuration_not_verified")
        self.assertTrue(issues)
        self.assertNotIn("nominal conversion:", output)


if __name__ == "__main__":
    unittest.main()
