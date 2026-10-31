# SparkFun LSM6DSO integration / 1.1.17

[中文](../zh/IMU_LSM6DSO.md) · [Full setup guide](GETTING_STARTED.md) · [Firmware explanation](FIRMWARE.md)

Target: **SparkFun SEN-18020, LSM6DSO Qwiic**, replacing the planned LSM6DS3TR-C. Connect the IMU only to the child tag. The counselor needs updated software to display motion, but no sensor.

## Wiring

Disconnect tag USB power before wiring and inspect before reconnecting:

| SparkFun | XIAO ESP32S3 tag |
|---|---|
| 3V3 | 3V3, never 5V |
| GND | GND |
| SDA | D4 / GPIO5 |
| SCL | D5 / GPIO6 |

Use a Qwiic-to-jumper cable or reliably soldered through-hole connections. Leave other pins/jumpers at their defaults; no interrupt wires are needed. This I2C bus does not share the Wio SPI pins. References: [SparkFun product documentation](https://www.sparkfun.com/sparkfun-6-degrees-of-freedom-breakout-lsm6dso-qwiic.html), [official hardware overview](https://learn.sparkfun.com/tutorials/qwiic-6dof-lsm6dso-breakout-hookup-guide/hardware-overview).

## Build and update

Set `BG_WS` and `BG_KEY` using the full setup guide and run from the repository. Use fresh build directories:

```sh
"$BG_WS/.venv/bin/python" scripts/build.py --workspace "$BG_WS" --key "$BG_KEY" --build-dir "$BG_WS/build-bg-tag1-imu" --role tag --tag-id 1 --imu
"$BG_WS/.venv/bin/python" scripts/build.py --workspace "$BG_WS" --key "$BG_KEY" --build-dir "$BG_WS/build-bg-counselor-imu" --role counselor
```

`--imu` is tag-only. It enables `CONFIG_BUDDYGUARD_IMU`, the Zephyr sensor/I2C services and `conf/lsm6dso.overlay`. Tags built without it continue reporting UNKNOWN; the counselor needs neither the option nor an IMU. Old build profiles without the new `imu` field may reject reuse: use a new directory rather than editing the marker.

The service probes default address `0x6B`, then alternate `0x6A`; WHO_AM_I register `0x0F` must return `0x6C`. Both device nodes are deferred; only the detected device initializes. Sign with the existing team key and follow the full guide's **BLE update** procedure with the correct role artifact. Do not erase already-provisioned boards merely to add the sensor.

## Inspect measurements

Set the current USB port, then run:

```sh
.venv/bin/python tools/bg_serial.py --port "$TAG_PORT" command bg imu
.venv/bin/python tools/bg_serial.py --port "$TAG_PORT" --log evidence/raw/my-imu.log monitor --seconds 40
```

`bg imu` reports enabled/available, address, WHOAMI, motion, sample count/age/error, and six axis values. Expect `enabled=1 available=1`, WHOAMI `0x6c`, increasing sample count and age well below 1000 ms. When unavailable, axis fields may retain the last valid values; they are not current valid measurements. Disabled roles correctly report enabled0.

Units: 1000 `accel_mg` is approximately 1g; 1000 `gyro_mrad_s` is 1 rad/s. Gravity remains present at rest, so acceleration should not be expected to be zero. Small gyro bias is possible; precision calibration is not implemented.

## Sampling and classification

A dedicated lower-priority thread in `src/sensors/imu.c` owns IMU I2C. Main, shell and LoRa read a mutex-protected snapshot; I2C operations never occur while holding that mutex.

- ODR52 Hz, software polling every 40 ms (approximately 25 Hz, with scheduling/I2C overhead).
- Accel range +/-4g, gyro +/-250 degrees/s, polling without interrupt wires.
- Status register `0x1E` must indicate new accel and gyro data before fetching; absence of fresh data does not refresh the timestamp.
- One second of continuous valid samples warms up from UNKNOWN. A gap over one second restarts warm-up.
- Activity means the sum of absolute axis-to-axis acceleration changes exceeds 150 mg, or an absolute gyro axis exceeds 300 mrad/s.
- MOVING holds for two seconds after activity, then becomes STILL. These laboratory thresholds are not a model validated on children.
- Read failure immediately produces UNKNOWN; data older than 1000 ms also produces UNKNOWN. A missing IMU does not block wireless service or demand image rollback.

Pure C logic lives in `src/protocol/motion.c`. Native tests cover gravity, warm-up, activity hold, stale sampling and uint32 clock wrap.

## LoRa demonstration

Protocol1 remains 12 bytes, using reserved byte8: `0=UNKNOWN`, `1=STILL`, `2=MOVING`. STATUS, SOS and QUERY REPLY capture the current available state at frame creation; retries reuse that snapshot. Clasp and battery remain unknown/unavailable. Raw six-axis values are available in local logs, not sent in this LoRa frame.

Rest the IMU: the counselor should receive periodic `motion=STILL`. Gently move/rotate the sensor itself for about 15 seconds to span a roughly ten-second STATUS cycle; expect MOVING. Rest it again: local state settles after about two seconds, then the next STATUS updates the counselor. Do not tug on wiring or disconnect live wires.

You can also issue tag `bg sos` while moving, or counselor `bg query 1` for a current snapshot. Close the corresponding serial monitor before issuing commands; check reception and ACK, not only queue success. A 1.1.16 counselor hardcodes UNKNOWN in its logs, so update it to interpret this field correctly.

STILL only means no recent significant device motion was detected. It is not a danger/fall decision; the IMU supplies neither location nor reliable distance. Consult the dated hardware evidence for actual observations; compilation and synthetic tests do not establish real movement behavior.
