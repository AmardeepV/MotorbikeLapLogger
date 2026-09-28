# Bluetooth-only isolation test

**Archived investigation:** debugging was paused at the user's request. These
diagnostic environments are no longer in the active PlatformIO configuration.
Use `nicla_live_ble` for normal operation. The pre-restoration source snapshot
is in `captures/debug-checkpoints/ble-debug-before-live-restore.tar.gz`.

## Why this test is different

The longer run shows continuing failures and a late cluster, not a startup-only
problem. Selected readings from the supplied screenshots:

| Activity time | Board uptime | Board drop count |
| --- | ---: | ---: |
| 10:17:47 | 680.0 s | 7 |
| 10:18:06 | 699.2 s | 8 |
| 10:21:06 | 879.0 s | 11 |
| 10:24:02 | 1055.7 s | 13 |
| 10:25:26 | 1138.5 s | 17 |
| 10:26:15 | 1188.0 s | 28 |
| 10:28:47 | 1339.5 s | 37 |
| 10:30:17 | 1430.4 s | 46 |

The first attached screenshot (09:57 events) is from the earlier five-minute run
and must not be merged into this board-uptime sequence. The reported 20-minute
test and 23.8-minute board uptime are different time windows: 46 is a since-boot
count, not an exact count over a verified 20-minute interval. There are subsequent
timeout messages after the 46-drop snapshot, so it is a lower bound by test end.

The later recorded maxima remain sensor 106 ms, poll 10 ms, notify/status <1 ms,
loop 157–158 ms. The transmit guard avoids the previous long application writes,
but has not solved connection stability. Available RSSI readings around -46 to
-51 dBm do not establish packet reliability or rule out interference. Retained
calibration and increasing uptime show no intervening board reboot in these
reported snapshots. Deferrals count retry checks, not dropped notifications.

## Install the separate diagnostic

Software verification: both `nicla_live_ble` and `nicla_ble_isolation` build
successfully. The isolation transport object references board power setup and
the public PMIC status read, with no BHY2 initialization/update calls. Connection
stability still requires the physical test below.

From the project, run this exact command (normal Upload still selects live firmware):

```sh
/Users/amardeep/.platformio/penv/bin/pio run -d /Users/amardeep/Desktop/MotorbikeLapLogger/Nicla/Nicla_Firmware -e nicla_ble_isolation -t upload
```

Alternatively choose PlatformIO Project Tasks -> nicla_ble_isolation -> Upload.
Use the existing iPhone app; no app rebuild is needed. Power-cycle the Nicla after
uploading so the sensor hub is not left running its previous configuration.
Reconnect to Nicla Motion. Activity must say `LINK_ONLY_TEST`.

Do not press CAL or LAP during this test. No lean angles will be shown: packets
explicitly mark measurements unavailable, rather than transmitting fake angles.
The diagnostic keeps the same service/characteristics, credit guard, 10 Hz
three-frame traffic target, connection-interval request, 1 ms scheduling pause,
and diagnostics. It skips BHY2 initialization and polling but retains board power
initialization and the same 30-second PMIC servicing. This tests host-side sensor
integration; it is not a claim that every physical sensor chip is powered off.

Keep the phone nearby, app visible, screen awake, and the same power arrangement.
Run for 10 minutes, or stop at the first timeout and share the disconnect and
adjacent Board uptime/Last link check entries. If it stays connected, report that
result; ten minutes without a failure is encouraging, not proof of stability.

- A timeout without sensor initialization/polling shows that work is not required
  to reproduce the failure; next isolate BLE controller/connection setup and RF.
- No timeout only shifts suspicion toward sensor integration or changed timing;
  compare with a normal build under the same conditions before assigning cause.

## Restore normal measurements

```sh
/Users/amardeep/.platformio/penv/bin/pio run -d /Users/amardeep/Desktop/MotorbikeLapLogger/Nicla/Nicla_Firmware -e nicla_live_ble -t upload
```

Reconnect and calibrate normally. UI, normal LAP/STOP semantics, and the default
build selection are unchanged. No SD-card storage or GNSS is introduced.

## Result and next comparison: periodic notifications disabled

The subsequent screenshot confirms `LINK_ONLY_TEST` at 14:28:50, following
connection at 14:28:49. A timeout occurs at 14:29:14 and reconnection at 14:29:17.
The last health reading was four seconds old: uptime 24.1 s, zero prior drops,
sensor/poll/notify/status maxima below 1 ms, loop maximum 5 ms, 8350 transmit
deferrals, RSSI -40 dBm. This reproduces the failure without host sensor work.
It does not identify the controller, phone, power, or radio environment as the
cause, and the final four seconds are not represented in that health snapshot.

Upload the next comparison with PlatformIO Project Tasks -> nicla_ble_idle ->
Upload, or:

```sh
/Users/amardeep/.platformio/penv/bin/pio run -d /Users/amardeep/Desktop/MotorbikeLapLogger/Nicla/Nicla_Firmware -e nicla_ble_idle -t upload
```

Power-cycle, reconnect with the same app, and verify `IDLE_LINK_TEST` in Activity.
This retains the same connection settings, power servicing and health reads.
It sends an initial unavailable-data snapshot and status on connection, but
disables recurring motion/rates/session notifications. It is not completely
radio-silent: connection maintenance and app health/RSSI requests continue.

Keep the app visible and the same power/location arrangement. Do not press
CAL/LAP/STOP. Run ten minutes or stop at the first timeout; share the surrounding
Activity entries. No angles are expected and no app update is needed.
If it fails, continuous application notifications are not required to reproduce
the timeout. If it stays connected, compare streaming under identical conditions
before attributing the failure to notification traffic. Neither outcome alone
identifies a definitive root cause. Restore normal firmware using the command
above when finished.

## Idle result and independent-client comparison

The next screenshot confirms `IDLE_LINK_TEST` at 14:32:59. Connection at
14:32:57 times out at 14:33:04, followed by reconnection at 14:33:06.
The last health sample is two seconds old: uptime 8.1 s, zero prior drops,
zero TX deferrals, loop maximum 4 ms, other measured maxima below 1 ms,
RSSI -41 dBm. Recurring telemetry and host sensor processing are therefore
not necessary to reproduce this failure. The last sample does not measure
the final two seconds. CoreBluetooth error 6 is not a captured raw HCI reason.

Next comparison requires no new firmware: keep `nicla_ble_idle`, fully close
LapLogger to prevent its automatic reconnection, power-cycle the board, and
connect to Nicla Motion using Nordic nRF Connect for Mobile on the same iPhone.
Leave notifications off and the app visible. Observe for ten minutes or until
the first drop; share its connection log and elapsed time. Keep board power and
location the same. Failure here shows LapLogger-specific behavior is not needed;
success warrants comparing its discovery/subscriptions/health reads with our app,
without yet proving our app is the cause.

Source review: Arduino's host-board support article concerns BHY2Host/carrier
setups, and does not promise a fix for standalone iPhone timeouts. This project
does not use BHY2Host or Web Bluetooth. The installed core is 4.6.0 and ArduinoBLE
is 2.1.0, matching the latest releases listed when checked. Issue 125 reports
similar timeouts but is open, with no verified fix shown. The linked Apple forum
thread primarily concerns Nicla Voice; it does not establish an HTML filter fix.

- https://support.arduino.cc/hc/en-us/articles/4411202632466-If-Nicla-Sense-ME-sensors-can-t-be-read-by-a-host-board
- https://github.com/arduino/nicla-sense-me-fw/issues/125
- https://forum.arduino.cc/t/ble-not-stable-on-ios-ipados-macos/1379664
- https://github.com/arduino/ArduinoCore-mbed/releases
- https://github.com/arduino-libraries/ArduinoBLE/releases
- https://www.nordicsemi.com/Products/Development-tools/nRF-Connect-for-mobile

## Independent client reproduced the failure

The supplied nRF Connect CSV records connection at 14:41:43.328, completion of
descriptor discovery at 14:41:48.964, and disconnection at 14:41:58.800 (15.472 s
after connection). The user confirmed this was automatic. No notification
subscription is logged. No raw HCI disconnect reason is supplied. LapLogger
is therefore not required to reproduce the failure in this setup.

The next controlled comparison is `nicla_ble_defaults`. It is identical to the
idle firmware except for its status marker and omission of
`BLE.setConnectionInterval(12,24)`. Inspection of the installed ArduinoBLE
L2CAPSignaling.cpp shows this setting can trigger an immediate parameter-update
request when the initial interval lies outside the requested range. We have not
captured the actual negotiated interval or established that such a request
caused the failure. No timeout is extended in this comparison.

Upload PlatformIO Project Tasks -> nicla_ble_defaults -> Upload, or:

```sh
/Users/amardeep/.platformio/penv/bin/pio run -d /Users/amardeep/Desktop/MotorbikeLapLogger/Nicla/Nicla_Firmware -e nicla_ble_defaults -t upload
```

Keep LapLogger fully closed. Power-cycle and repeat the nRF Connect test with
notifications off, same phone/power/location, app visible, for ten minutes or
until the first disconnect. Export its log again. The readable UART TX/status
characteristic contains `DEFAULT_LINK_TEST` if build identification is needed;
do not enable notifications just to check it. No iOS rebuild is required.
Normal firmware and the previous idle comparison remain available unchanged.

### Default-parameter result

The next nRF Connect CSV records connection at 14:49:31.234 and disconnection at
14:51:47.623: 136.389 seconds. The user confirmed an automatic disconnect with
the Nicla powered over USB. Discovery completed at 14:49:33.933. Scanner Off at
14:51:27.704 is a separate log event, not the connection end. No raw disconnect
reason or negotiated connection parameters are included. Omitting the requested
interval did not eliminate failure; a single longer run is not evidence of a fix.

Next physical comparison: retain `nicla_ble_defaults`, the same iPhone/nRF
Connect and location, but use the user's already-connected battery with USB
fully unplugged. Power-cycle before connecting, leave LapLogger closed and
notifications off. Observe for ten minutes or first automatic disconnect and
export the log. This isolates the USB-powered arrangement; it does not assume
USB is faulty or that battery operation is automatically stable. A subsequent
test with a second phone (ideally Android) can separate phone/platform dependence
if failure persists. Avoid changing firmware and power/client simultaneously.

Clock inspection found an explicit crystal LF-clock selection in the installed
Nicla mbed_config.h and a 20 ppm default in nrf5x_lf_clk_helper.h. This is not
verification of physical oscillator accuracy or the compiled controller's timing;
no speculative clock changes have been made.
