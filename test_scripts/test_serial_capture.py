import io
import unittest
from contextlib import redirect_stdout

from capture_nicla_serial import save_stream
from unittest.mock import patch


class FakeSerial:
    def __init__(self, chunks):
        self.chunks = list(chunks)

    @property
    def in_waiting(self):
        return len(self.chunks[0]) if self.chunks else 0

    def read(self, _size):
        return self.chunks.pop(0) if self.chunks else b""


class SerialCaptureTests(unittest.TestCase):
    def test_split_markers_and_raw_bytes_preserved(self):
        chunks = [b"# NICLA_DIAG", b"NOSTIC_V1\r\n4,1,0,0,0,8192\r\n# EN",
                  b"D,accel_events=1\r\n"]
        output = io.BytesIO()
        self.assertEqual(save_stream(FakeSerial(chunks), output), "complete")
        self.assertEqual(output.getvalue(), b"".join(chunks))

    def test_error_file_is_retained(self):
        content = b"# NICLA_DIAGNOSTIC_V1\n# ERROR,configuration_read,4\n"
        output = io.BytesIO()
        self.assertIn("error", save_stream(FakeSerial([content]), output))
        self.assertEqual(output.getvalue(), content)

    def test_timeout_is_not_success(self):
        self.assertIn("timed out", save_stream(FakeSerial([]), io.BytesIO(), timeout=0))

    def test_quaternion_cues_and_raw_preservation(self):
        chunks = [b"# NICLA_QUATERNION_V1\n"]
        chunks += [f"37,{i},{t},0,0,0,16384,0\n".encode() for i, t in enumerate(
            (0, 10000000, 15000000, 22000000, 27000000, 32000000, 37000000, 44000000, 49000000), 1)]
        chunks += [b"# END,quaternion_events=9\n"]
        output, prompts = io.BytesIO(), io.StringIO()
        with redirect_stdout(prompts):
            self.assertEqual(save_stream(FakeSerial(chunks), output, quaternion_test="roll"), "complete")
        self.assertEqual(output.getvalue(), b"".join(chunks))
        self.assertEqual(prompts.getvalue().count("LEAN RIGHT:"), 1)
        self.assertEqual(prompts.getvalue().count("LEAN LEFT:"), 1)

    def test_wrong_firmware_mode_is_not_success(self):
        output = io.BytesIO()
        result = save_stream(FakeSerial([b"# NICLA_DIAGNOSTIC_V1\n"]), output, quaternion_test="roll")
        self.assertIn("wrong firmware", result)

    def test_lean_waits_for_calibration_and_retains_raw(self):
        chunks = [b"# NICLA_LEAN_V1\nL,5000000,CALIBRATING,,,0\n",
                  b"4,1,5100000,0,0,8192,0,0\nL,8000000,VALID,0.0,0.0,1\n",
                  b"L,13000000,VALID,25.0,0.0,1\n",
                  b"L,113000000,VALID,0.0,0.0,1\n# END,quaternion_events=1\n"]
        output, prompts = io.BytesIO(), io.StringIO()
        with redirect_stdout(prompts):
            result = save_stream(FakeSerial(chunks), output, lean_test=True)
        self.assertEqual(result, "complete")
        self.assertEqual(output.getvalue(), b"".join(chunks))
        self.assertEqual(prompts.getvalue().count("CALIBRATED:"), 1)
        self.assertEqual(prompts.getvalue().count("LEAN RIGHT about"), 1)
        self.assertLess(prompts.getvalue().index("CALIBRATING:"), prompts.getvalue().index("CALIBRATED:"))

    def test_lean_invalid_or_late_calibration_is_not_success(self):
        for lines in (b"L,8000000,VALID,0,0,1\nL,9000000,STALE,,,1\n",
                      b"L,118000000,VALID,0,0,1\n# END,x=1\n",
                      b"L,5000000,CAPTURE_FAULT,,,0\n",
                      b"L,5000000,VALID,nan,0,1\n",
                      b"L,5000000,INVALID,1,2,0\n"):
            with redirect_stdout(io.StringIO()):
                result = save_stream(FakeSerial([b"# NICLA_LEAN_V1\n"+lines]), io.BytesIO(), lean_test=True)
            self.assertNotEqual(result, "complete")

    def test_lean_stopped_stream_hides_angles(self):
        tick = iter(i*0.1 for i in range(1000))
        prompts = io.StringIO()
        with patch("capture_nicla_serial.time.monotonic", side_effect=lambda: next(tick)), redirect_stdout(prompts):
            result = save_stream(FakeSerial([b"# NICLA_LEAN_V1\nL,8000000,VALID,1,2,1\n"]), io.BytesIO(), lean_test=True)
        self.assertIn("live stream stopped", result)
        self.assertIn("angles unavailable", prompts.getvalue())

    def test_rotation_cues_follow_sensor_time_and_preserve_data(self):
        chunks = [b"# NICLA_DIAGNOSTIC_V1\n"]
        chunks += [f"13,{i},{t},0,0,0\n".encode()
                   for i, t in enumerate((100000, 10000000, 13000000, 20000000, 23000000, 24000000), 1)]
        chunks += [b"# END,gyro_events=6\n"]
        output, prompts = io.BytesIO(), io.StringIO()
        with redirect_stdout(prompts):
            self.assertEqual(save_stream(FakeSerial(chunks), output, rotation_x=True), "complete")
        self.assertEqual(output.getvalue(), b"".join(chunks))
        self.assertEqual(prompts.getvalue().count("\aTURN:"), 1)
        self.assertEqual(prompts.getvalue().count("RETURN:"), 1)
        self.assertEqual(prompts.getvalue().count("HOLD:"), 3)


if __name__ == "__main__":
    unittest.main()
