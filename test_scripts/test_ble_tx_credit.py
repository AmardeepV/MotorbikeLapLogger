"""Run with python3 -m unittest discover -s test_scripts -p test_ble_tx_credit.py."""
import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('credit_patch', ROOT/'Nicla/Nicla_Firmware/scripts/ble_tx_credit.py')
patch = importlib.util.module_from_spec(spec)
spec.loader.exec_module(patch)

class CreditTests(unittest.TestCase):
    def test_patch_is_verified_and_idempotent(self):
        header = ROOT/'Nicla/Nicla_Firmware/.pio/libdeps/nicla_live_ble/ArduinoBLE/src/utility/HCI.h'
        source = header.read_text()
        updated = patch.patched_header(source)
        self.assertEqual(patch.patched_header(updated), updated)
        with self.assertRaises(RuntimeError):
            patch.patched_header(source+'// unknown version\n')

    def test_real_accessor_all_credit_states(self):
        # Compile the exact injected method and exhaustively check controller
        # startup, exhaustion, reserved control credit, and available capacity.
        source = '''#include <cassert>
struct HCIProbe {
unsigned char _maxPkt, _pendingPkt;
''' + patch.ACCESSOR + '''
};
int main() {
 for(int capacity=0;capacity<256;++capacity) for(int used=0;used<256;++used) {
  HCIProbe h{(unsigned char)capacity,(unsigned char)used};
  bool ready=h.niclaNotificationReady();
  if(capacity==0 || used>=capacity) assert(!ready);
  else if(capacity==1) assert(ready);
  else assert(ready == (capacity-used>=2));
 }
}
'''
        with tempfile.TemporaryDirectory() as d:
            cpp=Path(d)/'credit.cpp';cpp.write_text(source)
            exe=Path(d)/'credit'
            subprocess.run(['c++','-std=c++11',str(cpp),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

if __name__=='__main__': unittest.main()
