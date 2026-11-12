# Pass test: tag low-power profile (Feature 2, validated with the PPK2)

Written before the firmware change. Requirement:

> Cut the tag's **average current by at least 10 %** without changing what it does: BLE presence, LoRa STATUS/SOS/QUERY, missing/recovered detection and impact SOS all still work.

Change under test (`build.py --low-power`, version 1.6.6):

1. ESP32-S3 CPU clock **240 MHz → 80 MHz** (`conf/lowpower.overlay`).

Revision 2: iteration 1 (1.6.5) also raised the BLE presence advertising interval from 100–150 ms to 1000–1500 ms. Its behaviour check failed P3/P4 (two false `BLE MISSING` in about a minute, `RECOVERED` 10 s late), so that change was reverted.

## Setup (same for before and after)

- Tag T01: XIAO ESP32S3 + Wio-SX1262 + LSM6DSO, antennas attached, lying still.
- **USB unplugged.** The PPK2 powers the tag in **source mode, 3300 mV**: VOUT → XIAO **3V3** pin, GND → XIAO **GND**. Never connect the PPK2 while the USB cable is plugged in.
- Counselor C01 powered within 1 m, so the tag's LoRa STATUS frames get ACKed.
- Power the tag, **wait 320 s** (the first 300 s after boot are the BLE maintenance window, which uses fast 30–60 ms connectable advertising), then record **60 s**. One command does all three and switches the PPK2 off at the end:
  `~/ppk2-venv/bin/python tools/bg_ppk2_steady.py 3300 320 60 docs/closed-loop/captures/<before|after>.csv`
- Before = firmware 1.6.4 (`build-cl-tag`). After = firmware 1.6.6 (`build-cl-tag-lowpower`).

## Pass criteria

| # | Check | Pass if | Source |
| --- | --- | --- | --- |
| P1 | Average current | after ≤ 0.90 × before (report both in mA) | PPK2 CSV |
| P2 | Peaks still periodic | LoRa TX peaks every 10 s ± 1 s in both captures | PPK2 CSV |
| P3 | Presence | counselor shows no `BLE MISSING` for T01 during 120 s of normal operation | counselor log |
| P4 | Demo regression | `demo.sh` (USB connected): SOS → ACTION; heartbeat off → MISSING ≤ 10 s → QUERY answered; heartbeat on → RECOVERED | serial logs |
| P5 | Impact still works | one tap → one `IMPACT` and one counselor `ACTION` | serial logs |

## Result

**PASS (P1–P5), 2026-11-11.** Before = tag T01 on 1.6.4 (`build-cl-tag`). After = 1.6.6 (`build-cl-tag-lowpower`),
flashed over BLE: `FLASH OK: 1.6.6 running and confirmed`. Both PPK2 runs used the same setup and command:
USB unplugged, 3300 mV into 3V3, `bg_ppk2_steady.py 3300 320 60`, tag lying still, counselor C01 on a phone
charger within 1 m.

| | Before (1.6.4, 240 MHz) | After (1.6.6, 80 MHz) |
| --- | --- | --- |
| Average current | **74.881 mA** | **63.243 mA** |
| Median / p99 | 73.477 / 120.248 mA | 62.026 / 109.485 mA |
| Peak sample (10 µs) | 253.1 mA | 250.7 mA |
| LoRa TX events (50 ms mean) | 6, 111.9–115.5 mA, about 105 ms wide | 6, 100.9–104.4 mA, about 105 ms wide |
| TX intervals | 10.05, 10.03, 10.20, 10.01, 10.18 s | 10.01 s × 5 |
| Samples | 6,000,000 of 6,000,000, 0 out of range | 6,000,000 of 6,000,000, 0 out of range |

| # | Measured | Verdict |
| --- | --- | --- |
| P1 | 63.243 / 74.881 = 0.845, so **−15.5 %** (limit: ≤ 0.90 × before = 67.39 mA) | PASS |
| P2 | 6 TX events in each 60 s capture, intervals 10.01–10.20 s (limit 10 ± 1 s) | PASS |
| P3 | Run 2: the counselor was rebooted with the tag already out of maintenance. Roster `ONLINE tag=T01 seq=458 maintenance=0` at counselor uptime 4.142 s, then **0 `BLE MISSING`** through 3:20 (196 s), with 20 STATUS frames received at a 10 s cadence | PASS |
| P4 | Two-board demo on 1.6.6 (`version=1.6.6 … confirmed=1`): SOS → `ACTION` at 29:18.580; heartbeat off → `BLE MISSING age_ms=9853` (≤ 10 s) → `QUERY complete … attempt=1` at 29:36.111; heartbeat on → `RECOVERED` at 29:55.880 | PASS |
| P5 | Run 2: one knock → one `IMPACT src=polled axis=z polled_delta_mg=1051` → SOS seq 105, ACK on attempt 1 → one counselor `ACTION`. The tag's `bg imu` counter went from 2 to 3 (`impacts=3`) | PASS |

**Run 1 (P3/P5 first attempt, not used for the verdict).** P5 measured one knock → 2 IMPACTs and 2 ACTIONs:
`IMPACT src=polled axis=x 1071 mg` at tag uptime 08:27.042, then `IMPACT src=imu axis=z 511 mg` at 08:30.182,
3.14 s later, just after the 3 s lockout. Between the two, the tag logged `motion=MOVING`,
`accel_mg=(-559,0,515)`, `gyro_mrad_s=(-3152,1487,-1452)`: the board rotated about 3 rad/s and tilted about 45°
after the knock. The second detection was a real second motion event, not a firmware fault. Impact detection
does not depend on the CPU clock, and the 80 MHz change does not touch it. Run 1 also could not prove P3: it had
0 MISSING, but the roster's `ONLINE` line came before the log started, so the log did not show that T01 was
being tracked. Run 2 rebooted only the counselor so that `ONLINE` was captured, and the board was held flat for
the knock.

**Instrument check.** The BENCH.md known-answer check (1 kΩ at 3300 mV ≈ 3.3 mA) was **not done**: no resistor
was on hand. Instead, `bg_ppk2_steady.py` marks a run INVALID if fewer than 95 % of the expected samples arrive
or any sample is outside 0–1 A. Both runs passed that check. That check catches a broken stream but not a
plausible scale error, so the absolute mA values are not independently verified. The before/after ratio for P1
comes from the same instrument, script and setup. The first "before" attempt, made with the earlier
single-loop reader, lost about 82 % of the samples and decoded the rest as 9.3–10 A. It is kept as
`lowpower-before-INVALID-2026-11-11.csv` and not used.

**Notes.**
- In both runs the PPK2 logged `ERROR:root: … port that is not open` at exit. That is `PPK2_MP.__del__` calling
  `stop_measuring()` after the script's `finally` had already switched the output off and closed the port.
- P2 uses `tools/bg_ppk2_peaks.py <csv> 15`: a 50 ms moving average, with events above median + 15 mA merged
  within 1 s. With +10 or +25 mA it finds the same 6 events and the same intervals.
- P4 is from the 2026-11-11 two-board demo run before this session (`demo.sh`, both boards on USB). It was not
  repeated.

Files in `captures/`:
- `lowpower-before-2026-11-11.csv` and `lowpower-after-2026-11-11.csv`: PPK2, 1 ms bins of mean and max µA.
- The matching `.log` (script output) and `-peaks.txt` (P2 analysis) for each.
- `lowpower-flash-1.6.6-2026-11-11.log`.
- `lowpower-demo-2026-11-11/` (P4 logs).
- `lowpower-run1-2026-11-11/` and `lowpower-run2-2026-11-11/`: tag and counselor consoles, `times.txt` with the
  cue and knock times, and `bg imu` after each run.
- `lowpower-before-INVALID-2026-11-11.*`: the discarded first attempt.
