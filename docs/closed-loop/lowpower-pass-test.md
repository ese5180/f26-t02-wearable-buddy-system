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

Fill in: before/after average and peak current, capture files under `docs/closed-loop/captures/`, P2–P5 outcomes.
