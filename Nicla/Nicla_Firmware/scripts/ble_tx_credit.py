"""Version-checked ArduinoBLE 2.1.0 read-only TX credit accessor.

Post build-configuration hook: dependencies are installed before this script runs.
No library flow-control loop or controller credit accounting is modified.
"""
from hashlib import sha256
from pathlib import Path

ORIGINAL_SHA = '1370e57e9bb3df8ad08bd66fede11c351ee28a2332f99a773ea77be5bf45fa9f'
ACCESSOR = '''  // Nicla application notifications must not enter the blocking credit wait.
  // Leave one credit for ATT/control traffic when the controller has >1 buffer.
  bool niclaNotificationReady() const {
    return _maxPkt > 0 && _pendingPkt < _maxPkt &&
      (_maxPkt == 1 || (_maxPkt - _pendingPkt) > 1);
  }
'''

def patched_header(source):
    original = source.replace(ACCESSOR, '', 1)
    if sha256(original.encode()).hexdigest() != ORIGINAL_SHA:
        raise RuntimeError('Unexpected ArduinoBLE HCI.h; review TX-credit integration before building')
    return original.replace('public:\n', 'public:\n' + ACCESSOR, 1)

if 'Import' in globals():
    Import('env')
    path = Path(env.subst('$PROJECT_LIBDEPS_DIR')) / env.subst('$PIOENV') / 'ArduinoBLE/src/utility/HCI.h'
    source = path.read_text()
    updated = patched_header(source)
    if updated != source:
        path.write_text(updated)
