import io
import unittest
from contextlib import redirect_stdout
from analyze_nicla_motion import analyze, parse_motion
from capture_nicla_serial import save_stream
from test_serial_capture import FakeSerial


def fixture():
    lines = ['# NICLA_MOTION_V1','sensor_id,sequence,host_us,x_raw,y_raw,z_raw,w_raw,accuracy_raw']
    for phase in ('start','end'):
        for sid,rate,full_range in ((4,50,4),(13,50,1000),(22,12.5,1),(37,50,1)):
            lines.append(f'# CONFIG,{phase},{sid},{rate},0,{full_range},0')
    for sid,step in ((4,20000),(13,20000),(22,80000),(37,20000)):
        for seq,t in enumerate(range(0,120000000,step),1):
            values='0,0,0,16384,0' if sid==37 else '0,0,8192,0,0' if sid==4 else '0,0,0,0,0'
            lines.append(f'{sid},{seq},{t},{values}')
    for seq,t in enumerate(range(0,120000000,100000),1):
        status='VALID' if t>=8000000 else 'STARTUP' if t<5000000 else 'CALIBRATING'
        cal=1 if status=='VALID' else 0
        lines.append(f'L,{t},{status},'+('0,0' if cal else ',')+f',{cal}')
        values='0,0,0,0,0,0' if cal else ',,,,,'
        lines.append(f'M,{seq},{t},{status},{values},0,0,0,0,{cal}')
    lines.append('# END,accel_events=6000,gyro_events=6000,magnetometer_events=1500,quaternion_events=6000,queue_dropped=0,malformed=0,max_poll_gap_us=20000')
    return ('\n'.join(lines)+'\n').encode()


class MotionCaptureTests(unittest.TestCase):
    def test_complete_run_with_all_nine_axes(self):
        text,issues=analyze(b'boot\xff\n'+fixture())
        self.assertEqual(issues,[])
        self.assertIn('Sensor 22: 1500',text)

    def test_missing_raw_event_and_missing_end_fail(self):
        raw=fixture().replace(b'22,2,80000,0,0,0,0,0\n',b'')
        self.assertTrue(analyze(raw)[1])
        self.assertTrue(analyze(raw.split(b'# END')[0])[1])

    def test_valid_row_must_have_finite_values_and_fresh_samples(self):
        good='M,1,8000000,VALID,1,-2,3,4,5,6,100,200,300,2,1'
        self.assertEqual(parse_motion(good)[3],1)
        for bad in (good.replace('1,-2,3','nan,-2,3'),good.replace('100,200,300','100001,200,300'),good.replace(',2,1',',1,1')):
            with self.assertRaises(ValueError):parse_motion(bad)

    def test_unavailable_values_must_be_blank(self):
        parse_motion('M,1,8000000,INVALID,,,,,,,0,0,0,1,1')
        with self.assertRaises(ValueError):parse_motion('M,1,8000000,INVALID,1,2,3,4,5,6,0,0,0,1,1')

    def test_guided_motion_mode_and_bytes_preserved(self):
        content=(b'# NICLA_MOTION_V1\nL,8000000,VALID,0,0,1\n'
                 b'M,1,8000000,VALID,0,0,0,0,0,0,0,0,0,0,1\n'
                 b'L,113000000,VALID,0,0,1\n# END,x=1\n')
        output,prompts=io.BytesIO(),io.StringIO()
        with redirect_stdout(prompts):result=save_stream(FakeSerial([content]),output,motion_test=True)
        self.assertEqual(result,'complete')
        self.assertEqual(output.getvalue(),content)
        self.assertIn('SLIDE:',prompts.getvalue())
        self.assertIn('Estimated forward acceleration',prompts.getvalue())

    def test_live_mode_never_opens_recording_file(self):
        import types
        from unittest.mock import patch, MagicMock
        import capture_nicla_serial as capture
        serial = types.ModuleType("serial")
        serial.SerialException = OSError
        serial.Serial = MagicMock()
        serial.tools = types.ModuleType("serial.tools")
        serial.tools.list_ports = types.SimpleNamespace(comports=lambda: [])
        modules = {"serial":serial,"serial.tools":serial.tools}
        def stream(connection, output, **kwargs):
            self.assertIsInstance(output,capture.DiscardOutput)
            self.assertTrue(kwargs['motion_test'])
            output.write(b'sensor data'); output.flush()
            return 'complete'
        with patch.dict('sys.modules',modules), patch('sys.argv',['capture','--port','fake','--live-motion']), \
             patch('builtins.input',return_value=''), patch.object(capture,'save_stream',side_effect=stream), \
             patch.object(capture.Path,'open',side_effect=AssertionError('must not open a file')), \
             patch.object(capture.Path,'mkdir',side_effect=AssertionError('must not create directory')), \
             redirect_stdout(io.StringIO()) as screen:
            self.assertEqual(capture.main(),0)
        self.assertIn('No data file was created',screen.getvalue())
        self.assertNotIn('Saving to:',screen.getvalue())

if __name__=='__main__':unittest.main()
