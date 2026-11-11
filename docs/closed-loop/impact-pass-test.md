# Pass test: IMU impact → automatic SOS (Feature 1, validated with the Saleae)

Written before the firmware change. Requirement (one sentence):

> When the tag's LSM6DSO sees an acceleration change of at least **1000 mg on any axis between two consecutive accelerometer samples**, the tag drives **D0 high for 50 ms ± 5 ms** and queues **one LoRa SOS**, and it ignores further impacts for **3 s**.

Revision 2 (after the first loop run, see the README): the comparison runs inside the IMU on every **417 Hz** sample (LSM6DSO wake-up slope detector, latched, read every 40 ms), because a knock lasts about 1 ms. Revision 3: threshold 1500 → 1000 mg, because at 1500 mg only 1 of 3 hard desk knocks fired.

## Setup

- Tag T01: XIAO ESP32S3 + Wio-SX1262 + SparkFun LSM6DSO (SDA = D4, SCL = D5), firmware built with `--imu`, version 1.6.4. USB connected for the serial log.
- Counselor C01 powered within 1 m, its serial log recorded.
- Saleae, 4 MS/s, ground to tag GND:
  - ch0 = **D0 / GPIO1** (impact marker)
  - ch1 = **D4 / GPIO5** (I2C SDA)
  - ch2 = **D5 / GPIO6** (I2C SCL)
- One 60 s capture. Board flat and untouched for the first 20 s, then three single taps of the board edge on the desk, at least 5 s apart, then one double tap (two taps less than 1 s apart).

## Pass criteria

| # | Check | Pass if | Source |
| --- | --- | --- | --- |
| T1 | No false trigger at rest | 0 rising edges on ch0 in the first 20 s | Saleae |
| T2 | Marker width | every ch0 pulse is 50 ms ± 5 ms | Saleae |
| T3 | One pulse per tap | 3 single taps → 3 pulses | Saleae + tag log `IMPACT` lines |
| T4 | Lockout | the double tap → exactly 1 pulse | Saleae |
| T5 | Reaction time | each ch0 rising edge is ≤ 5 ms after the last ch2 (SCL) edge before it | Saleae |
| T6 | SOS reaches the counselor | each pulse → tag log `SOS queue result=0`, and counselor log `ACTION: counselor check tag T01, SOS received` within 2 s | serial logs |
| T7 | Sampling not disturbed | median gap between I2C bursts 40–50 ms | Saleae |

Tag log lines to expect: `IMPACT axis=<x|y|z> delta_mg=<n> (threshold 1000); SOS queue result=0`.

## Result

Fill in after the capture: date, firmware version, capture files under `docs/closed-loop/captures/`, and the measured value for each row.
