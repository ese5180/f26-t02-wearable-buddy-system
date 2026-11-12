# 3.7 Closing the Loop: HIL testing + AI

Two tag features were built with an AI agent in a closed loop: **write the pass test first → change the firmware → build → flash → measure with real instruments → prove or revert**. The loop follows Matt Starfield's [closed-loop-starter](https://github.com/SolidPD/closed-loop-starter). On macOS the agent (Claude Code) drives Logic 2's MCP server and the PPK2 directly, without Hapimatic, as the starter kit recommends for Macs.

| Feature | Requirement | Instrument | Pass test | Status |
| --- | --- | --- | --- | --- |
| 1. Impact → automatic SOS | ≥ 1000 mg single-axis change between 417 Hz IMU samples → 50 ms marker on D0 + one LoRa SOS, 3 s lockout | Saleae (D0, SDA, SCL) | [impact-pass-test.md](impact-pass-test.md) | iterations 1–2 tuned on the bench (see below); iteration 3 (1.6.4): **PASS** T1–T7 (Saleae run 2026-11-11) |
| 2. Low-power profile | tag average current −10 % or better, same behaviour | PPK2 (source 3300 mV) | [lowpower-pass-test.md](lowpower-pass-test.md) | iteration 2 (1.6.6, 80 MHz only): **PASS** P1–P5, 74.88 → 63.24 mA (−15.5 %) |

## Feature 1: impact detection → automatic SOS

A hard knock or a fall shows up as a sudden jump in acceleration. The concept already plans "possible-fall alerts", so the tag now raises one by itself.

- **Detection happens inside the IMU.** The accelerometer runs at **417 Hz** (`conf/lsm6dso.overlay`), and `wakeup_init()` in `src/sensors/imu.c` turns on the LSM6DSO's wake-up detector: on every sample it subtracts the previous sample (slope filter) and, when any axis exceeds `WK_THS` = 1000 mg / 62.5 mg = 16, latches `WU_IA` and the axis bits in `WAKE_UP_SRC` (0x1B, `LIR=1`, cleared on read). The IMU thread reads that register every 40 ms, so no INT wire is needed.
- `src/sensors/motion.c`, `bg_impact_update()`: pure logic, unit-tested on the host. It accepts the IMU's flagged axes, also checks the change between the 25 Hz polled samples, and applies the 3 s lockout. The first sample after a gap of more than 1 s only re-initializes, so a stale comparison cannot fire.
- `src/sensors/imu.c`, `check_impact()`: runs in the IMU thread right after each I2C read. On an impact it:
  1. pulses `zephyr,user impact-gpios` (XIAO **D0 / GPIO1**) high for 50 ms through a delayable work item, so the IMU thread never blocks;
  2. calls `bg_radio_sos()`, the same queue the `bg sos` shell command uses, so the counselor shows `ACTION: counselor check tag T01, SOS received`;
  3. logs `IMPACT src=<imu|polled> axis=<x|y|z> ...`. `bg imu` now also prints `impacts=` and `last_impact_mg=`.
- `conf/lsm6dso.overlay` declares the marker pin. The feature only exists in `--imu` builds.

**Loop iteration 1 failed, which is why detection moved into the IMU.** Version 1.6.0 compared only the 25 Hz polled samples (accelerometer at 52 Hz). Serial-log check on 2026-11-11: 0 of 4 hard knocks on the desk fired; the IMU itself worked (`motion=MOVING`). A knock lasts about 1 ms; the 52 Hz output filter smooths it out and 40 ms polling usually misses it. Iteration 2 (1.6.2) uses the 417 Hz latched wake-up detector above: the setup line `IMU wake-up detector: threshold 1500 mg (WK_THS=24)` appeared and the first knock fired with `src=imu` while the polled samples only changed by 1295 mg, but only 1 of 3 hard knocks fired. Iteration 3 (1.6.4) lowers the threshold to 1000 mg (`WK_THS=16`); pass test T1 checks that rest and handling still give no false trigger.

Saleae wiring: ch0 = D0 (marker), ch1 = D4 (SDA), ch2 = D5 (SCL), ground to GND, 4 MS/s.

## Feature 2: low-power profile (`build.py --low-power`)

One change, in configuration only: `conf/lowpower.overlay` runs the ESP32-S3 at **80 MHz instead of 240 MHz**. Most of the tag's time is spent waiting (BLE, a 200 ms LoRa receive window, 40 ms IMU polling), and its work (SPI at 2 MHz, I2C at 100 kHz) does not need 240 MHz.

**Loop iteration 1 failed its behaviour check.** Version 1.6.5 also stretched BLE presence advertising from 100–150 ms to 1000–1500 ms (`CONFIG_BUDDYGUARD_PRESENCE_ADV_INTERVAL_MS`). On the bench (2026-11-11) impact SOS still worked 4 of 4 and LoRa QUERY/REPLY worked, but the counselor reported two false `BLE MISSING` events while the heartbeat was on, and `RECOVERED` came 10 s after the heartbeat returned instead of at once. The counselor needs a new presence sequence within 8 s, and at 1 s advertising it too often missed every frame of a sequence. Iteration 2 (1.6.6) keeps only the 80 MHz change; the Kconfig option stays, defaulting to the original 100 ms.

What does not change: BLE presence timing, LoRa STATUS every 10 s, SOS/QUERY/REPLY, the 8 s missing timeout, IMU sampling and impact detection.

Measurement: USB unplugged, PPK2 source mode 3300 mV into the XIAO 3V3 pin, wait 320 s (past the 300 s maintenance window), record 60 s with `tools/bg_ppk2_steady.py`. Before = 1.6.4 (`build-cl-tag`), after = 1.6.6 (`build-cl-tag-lowpower`).

## The prompts

The requirement prompts, based on the starter kit templates. In the bench sessions they were given together with the wiring and cue details for that run.

Feature 1 (from closed-loop-starter `prompts/feature-v1.md`):

```text
Requirement: when the tag's LSM6DSO sees an acceleration change of at least 1000 mg on any axis
between two consecutive accelerometer samples, the tag drives D0 high for 50 ms ± 5 ms and queues one LoRa SOS,
and ignores further impacts for 3 s.

Work the loop in AGENTS.md. The pass test is docs/closed-loop/impact-pass-test.md. Flash
build-cl-tag, capture D0/SDA/SCL with the logic2 MCP server while I tap the board on your cue,
analyse the exported CSV, and decide pass or fail for every row. Finish with a short report:
the measured numbers, the files you saved, and the commit.
```

Feature 2 (from `prompts/low-power-v1.md`):

```text
Requirement: cut the tag's average current by at least 10 % without changing what it does:
BLE presence, LoRa STATUS/SOS/QUERY, missing/recovered detection and impact SOS all still work.

Work the loop in AGENTS.md. The pass test is docs/closed-loop/lowpower-pass-test.md. Measure the
"before" on build-cl-tag (1.6.4), flash build-cl-tag-lowpower (1.6.6), measure the "after" under
the same conditions, then check the behaviour with demo.sh and one tap. Power the board off at the
end. Commit on a pass with the before and after numbers in the message.
```

## Results

Filled in from the instrument runs (files under [captures/](captures/)).

| Pass test | Measured | Pass? |
| --- | --- | --- |
| T1–T7 impact | 1.6.4, Saleae 4 MS/s: 0 false edges at rest; pulses 50.03–50.05 ms; 3 knocks → 3 pulses + 3 `IMPACT`; double knock → 1 pulse; marker 0.022–0.024 ms after the I2C read; SOS ACKed 165–332 ms (see [impact-pass-test.md](impact-pass-test.md)) | PASS |
| P1 average current | before **74.881 mA** (1.6.4) → after **63.243 mA** (1.6.6), **−15.5 %** (limit −10 %). PPK2 3300 mV, 60 s after 320 s settle, 6,000,000 samples each ([before](captures/lowpower-before-2026-11-11.csv), [after](captures/lowpower-after-2026-11-11.csv)) | PASS |
| P2 LoRa peaks | 6 TX events per capture, every 10.01–10.20 s (before) and 10.01 s (after); TX 50 ms mean 112–116 → 101–104 mA | PASS |
| P3 presence | counselor roster `ONLINE tag=T01 maintenance=0`, then 0 `BLE MISSING` in 196 s ([run 2](captures/lowpower-run2-2026-11-11/)) | PASS |
| P4 demo | SOS → ACTION; heartbeat off → MISSING after 9.85 s → QUERY answered (attempt 1); heartbeat on → RECOVERED ([demo logs](captures/lowpower-demo-2026-11-11/)) | PASS |
| P5 impact | one knock → one `IMPACT` (1051 mg) → one `ACTION`; `impacts` counter +1 (run 2). Run 1's double trigger came from the board tipping after the knock (see the pass test) | PASS |

PPK2 caveat: the resistor known-answer check in BENCH.md was not done. The runs rely on the script's
built-in sample-count and range check (both 100 %, 0 out of range).

## Limits

- Thresholds are bench values, not validated on children. A thrown or dropped bag can also trigger an impact SOS.
- The latched wake-up event is read every 40 ms, so the SOS starts up to about 40 ms after the knock. Wiring the LSM6DSO INT1 pin to a GPIO would make it immediate.
- The IMU board is on a cable, so knocks must hit the IMU board itself; a board that keeps tipping after a knock can trigger again once the 3 s lockout ends.
- The low-power numbers are for this bench tag (XIAO + Wio + IMU, no Wi-Fi build). A deployed tag would also need sleep modes, which the BLE and LoRa design here does not use yet.
