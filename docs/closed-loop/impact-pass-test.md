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

**PASS (T1–T7), 2026-11-11.** Tag T01 firmware 1.6.4 (`bg status`: `version=1.6.4 confirmed=1`), not reflashed.
One 60 s Saleae capture, 4 MS/s, ch0 = D0, ch1 = SDA, ch2 = SCL, I2C analyzer SDA = ch1 / SCL = ch2. The
knocks were on the red LSM6DSO board, cued by spoken prompts at about 22, 29, 36 s (single) and 44 s (double).
A first attempt the same day (knocks on the XIAO stack) was discarded at the operator's request and is not used.

**Substitution for T6:** the counselor C01 ran from a phone charger, so its serial log could not be recorded.
T6 uses the tag's log instead: each `IMPACT … SOS queue result=0` must be followed within 2 s by
`ACK tag=T01 type=3`, which the counselor sends only after it receives the SOS. Times are from the tag's own
uptime stamps (host arrival times are batched by USB and are not used).

| # | Measured | Verdict |
| --- | --- | --- |
| T1 | 0 rising edges on ch0 in 0–20 s (first edge at 23.604 s) | PASS |
| T2 | 4 pulses: 50.041, 50.030, 50.042, 50.048 ms (all within 45–55 ms) | PASS |
| T3 | 3 single knocks → 3 pulses at 23.604, 30.864, 38.168 s; 3 matching `IMPACT` lines (axis y, z, z) | PASS |
| T4 | double knock → 1 pulse at 45.685 s. The IMU latched the second tap (WAKE_UP_SRC = 0x0d at 46.496 s, 811 ms later), and the 3 s lockout suppressed it | PASS |
| T5 | ch0 rise after the last SCL edge: 0.022, 0.024, 0.023, 0.022 ms (≤ 5 ms) | PASS |
| T6 | every IMPACT has `SOS queue result=0`; `ACK tag=T01 type=3` after 283, 232, 165, 332 ms (seq 62, 64, 66, 67; attempt=1) | PASS (substituted check) |
| T7 | 1406 I2C bursts, start-to-start median 42.70 ms (min 42.69, max 43.71; idle gap 40.07 ms, burst 2.63 ms); unchanged within 3.5 s after each pulse | PASS |

Notes:
- Detection path: pulse 2 came from the in-IMU 417 Hz detector (`src=imu`). Pulses 1, 3 and 4 came first from
  the firmware's 40 ms polled comparison (`src=polled`, delta 1019, 1102, 1481 mg), which ran before the IMU
  latch was read.
- An SOS (`TX/ACK type=3 seq=59`) was sent at tag uptime 08:36.1. That is about 0.8 s before the capture
  started, so it falls outside the T1 window. Its trigger was not logged because the serial log began after it.
- The tag reported `motion=MOVING` (gyro ≤ 78 mrad/s) during the still window, but nothing triggered.
- Tag uptime maps to Saleae time as t = uptime − 516.932 s (fit on the 4 IMPACT lines, residual ≤ 1 ms).

Files in `captures/impact-run-2026-11-11/`: `impact-run-2026-11-11.sal` (Logic 2 capture), `digital.csv`
(raw ch0–ch2), `i2c.csv` (I2C analyzer table), `tag-serial.log` (host-time-stamped tag console),
`cues.log` (cue times), `analysis.txt` (output of `analyse.py`, `i2c_regs.py` and `tools/logic-edges-v1.py`).
