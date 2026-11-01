# Hardware validation — 29–31 October 2026

**Latest / 1.1.20 BLE and radio regression (2026-10-31):** both roles BLE-updated/self-confirmed; current C01 recovery bootloader passed wrong-key/corruption rejection and valid-fault watchdog revert. Same-file resume, interleaved reception and five-minute normal-operation results recorded: [English / 中文](wireless-fota-v1120-20261031.md).

**Previous / UART recovery (2026-10-31):** C01 MCUboot UART valid/wrong-key uploads, rejection, no-application recovery and restoration to1.1.19 passed. [UART evidence / 串口恢复验证](uart-recovery-20261031.md). T01 was not connected for this run.

**Previous / IMU stage (2026-10-31):** real LSM6DSO six-axis data and STILL/MOVING/STILL over LoRa verified. Tag1.1.17 and counselor1.1.18 active/confirmed. Counselor scan duplicate filtering corrected; 186 seconds without unintended loss, then deliberate loss/query/recovery passed. [IMU validation / IMU 验证](imu-validation-20261031.md).

**Previous modular baseline (2026-10-30):** [1.1.16 OTA/wireless/watchdog validation](modularization-20261030.md).

These are observations from two physical XIAO ESP32S3 + Wio-SX1262 B2B kits on the development Mac. Each board was backed up and provisioned once over USB. Subsequent normal application changes used BLE SMP; the later dated UART recovery section additionally records a bootloader-only USB update and MCUboot UART application uploads. USB otherwise supplied power and logs. Private keys, full flash backups and raw captures are excluded from Git.

## Secure update results

| Experiment | Observed result |
|---|---|
| Signed update, 1.0.0 → 1.1.0 | BLE transfer, test activation and active/confirmed 1.1.0 verified. |
| Signed update, 1.1.0 → 1.1.1 | New image booted unconfirmed, passed local health checks, then confirmed itself. |
| Wrong signing identity, candidate 1.3.0 | Full BLE upload and test request completed. Candidate was rejected at boot; confirmed 1.1.0 remained active. Offline verification with the team public key also rejected it. |
| Corrupted signed file, candidate 1.4.0 | A payload byte was changed after signing. Full BLE upload and test request completed; confirmed 1.1.1 remained active. Offline verification reported an invalid hash. |
| Correctly signed, deliberately failing 1.2.0 | Candidate actually booted, did not confirm, stopped feeding the watchdog, and reset. MCUboot restored confirmed 1.1.1. |
| Interrupted valid 1.1.3 transfer | Deliberately stopped at byte 87,258 (20%). Primary remained active/confirmed 1.1.1. Reconnecting with the same file resumed from byte 87,258. Final activation is recorded in the current-role section below. |

Wrong-key/corrupt candidate versions are synthetic negative-test headers, not releases. The fault image was separately compiled with both application and image version 1.2.0. Transport timeouts were retried and are not counted as signature rejection.

Selected serial evidence from the successful self-confirmation test:

```text
[00:00:04.000,000] BuddyGuard version=1.1.1 role=TAG tag=1 confirmed=0
[00:00:18.039,000] Presence seq=8 advertising=1 connected=0 maintenance=1 confirmed=0
[00:00:20.040,000] Image health check complete; confirm result=0
[00:00:20.040,000] Presence seq=9 advertising=1 connected=0 maintenance=1 confirmed=1
```

Selected serial evidence from the rollback test:

```text
[00:00:04.000,000] BuddyGuard version=1.2.0 role=TAG tag=1 confirmed=0
[00:00:04.030,000] FAULT INJECTION: unconfirmed image will stop feeding the watchdog
rst:0x7 (TG0WDT_SYS_RST),boot:0x2a (SPI_FAST_FLASH_BOOT)
[00:00:04.000,000] BuddyGuard version=1.1.1 role=TAG tag=1 confirmed=1
```

Offline rejection output is retained in [negative-offline-verification.log](negative-offline-verification.log). Tests used the unique team ECDSA-P256 root whose public key is in `keys/`. Hardware Secure Boot eFuses and flash encryption were not enabled. MCUboot application-signature checks do not provide protection against an attacker replacing the whole bootloader over physical USB.

## Current-role integration

Counselor 1.1.3 was installed over BLE on C01, initialized the SX1262 and BLE roster, and self-confirmed:

```text
BuddyGuard version=1.1.3 role=COUNSELOR tag=200 confirmed=0
Wio-SX1262 ready: 915 MHz SF7 BW125 CR4/5 power=4 dBm role=COUNSELOR
Counselor BLE roster started: demo timeout=8000 ms; maintenance grace=120 s
ONLINE tag=T01 seq=155 maintenance=1
Image health check complete; confirm result=0
```

T01 also booted 1.1.3 unconfirmed and self-confirmed at uptime 20.053 seconds. Its valid upload resumed from byte 87,258 and completed in 155.23 seconds after reconnection.

Closing maintenance on both 1.1.3 devices left advertisement sequences increasing, with no advertising error or watchdog reset during the 20/22-second regression captures. This fixes the 1.1.2 reserved-connection recycle callback bug found during mode switching.

Stopping T01 advertisements with `bg heartbeat off` produced this counselor sequence; restoring them produced recovery:

```text
[00:05:08.177,000] BLE MISSING tag=T01 age_ms=9867; requesting LoRa status
[00:05:08.307,000] QUERY tag=T01 seq=2
[00:05:22.183,000] RECOVERED tag=T01 seq=73 maintenance=0
```

The initial 1.1.3 native SX126x driver initialized but TX timed out without a peer ACK. The 1.1.4 LoRaMac-node trial also failed to complete TX. Neither attempt is a passed LoRa test.

Version 1.1.5 added explicit LDO selection and startup SPI readback. Both boards returned 0xffff rather than the configured private sync word 0x1424. Both refused confirmation, watchdog-reset, and reverted to their confirmed 1.1.4 role images. Version 1.1.7 additionally corrected the GPIO high-port register base for the pinned driver, but both boards still returned 0xffff and reverted. The GPIO override alone did not resolve communication.

The user confirmed both B2B connections. Software SPI diagnostic 1.1.8 stopped before main and required a reset to recover. Deferred initialization in 1.1.9 exposed a NULL-buffer crash in spi_bitbang.c; the armed watchdog reverted it. A one-line driver guard fixed the crash, but diagnostic 1.1.10 read 0x0000 and reverted. These later diagnostics were USB-staged in the secondary slot and then test-activated over BLE, not full wireless-transfer tests. The next control-pin probe (1.1.12) was compiled but not run. Both devices were rechecked as active/confirmed 1.1.4 when the user paused. See [the complete pause checkpoint](../docs/PAUSED-2026-10-29.md).

## Reproduction and limits

Use the [demo runbook](../docs/demo-runbook.md) for commands. Full bench captures are kept locally in `evidence/raw/`, including `v111-ota-boot.log`, `fault-rollback-boot.log`, `wrong-key-retry-client.log`, `wrong-key-verified-images.log`, `corrupt-retry-client.log`, `corrupt-verified-images.log`, and the 1.1.3 role logs. No private-key material or real child information is needed to reproduce the demo.

This is a bench test with one tag and one counselor. It does not establish outdoor range, distance accuracy, battery life, a two-child buddy link, or readiness for use with children. Wi-Fi is deferred by the team. IMU, clasp, motor and GNSS are not connected in this firmware stage; motion/clasp and battery fields explicitly report unknown/unavailable.

## Resumed validation — 30 October 2026

The user reassembled the boards into the documented B2B stack. Before upgrading, the original 1.1.4 tag received a LoRa ACK. A 1.1.13 diagnostic then read back 0x1424 and confirmed itself. This supersedes the earlier unresolved GPIO-base hypothesis: the trial override was unnecessary and was removed. Production builds use upstream GPIO definitions and hardware SPI, with no SDK patch applied.

Both roles then received **complete signed BLE transfers to 1.1.14**, booted unconfirmed, read back 0x1424, and self-confirmed at uptime 20.124 seconds. Independent SMP image-state queries reported active=true, confirmed=true and pending=false for 1.1.14 on both boards. This test did not USB-stage the final images.

| 1.1.14 bench case | Observation |
|---|---|
| Status + ACK | Counselor received STATUS; tag received matching ACKs. Retries were also exercised. |
| SOS | Tag type=3, seq=6; counselor printed an action for T01; tag received ACK on attempt 1. |
| Normal BLE loss | After heartbeat off, counselor reported `age_ms=8703`, sent QUERY seq=1 and received REPLY type=5. |
| Query response ACK | Tag logged received QUERY seq=1, sent REPLY and received its ACK on attempt 1. |
| BLE recovery | After heartbeat on, counselor reported `RECOVERED tag=T01 seq=46 maintenance=0`. |
| Manual roll call | `bg query 1` sent QUERY seq=2 and received REPLY seq=2. |
| Maintenance mode transition | Both roles continued advertising in normal mode with confirmed=1; no advertising error or reboot in the captures. |

Selected counselor log:

```text
[00:00:58.576,000] ACTION: counselor check tag T01, SOS received
[00:01:16.152,000] BLE MISSING tag=T01 age_ms=8703; requesting LoRa status
[00:01:16.309,000] QUERY tag=T01 seq=1
[00:01:16.447,000] RX tag=T01 type=5 seq=1 rssi=-17 snr=13 motion=UNKNOWN battery=unavailable
[00:01:32.159,000] RECOVERED tag=T01 seq=46 maintenance=0
```

The steady capture also contained **one STATUS delivery unconfirmed after three attempts**; later STATUS messages were acknowledged. These results prove the bench paths work, not a loss-free or safety-qualified link. Range and reliability testing remain outstanding.

An additional real issue appeared during concurrent BLE updating: a full 244-byte-packet upload lasts several minutes, exceeding the old 120-second maintenance grace. Version 1.1.15 makes this separate grace configurable (default 600 seconds) while preserving the normal 8-second timeout.

### Final 1.1.15 regression

Both boards received complete signed BLE transfers to 1.1.15 and independently reported active/confirmed 1.1.15 through SMP image queries. Both read back 0x1424 and self-confirmed. Final USB status checks showed the correct roles, `connected=0`, `maintenance=0`, `confirmed=1`; tag advertising was restored.

- Tag full BLE upload: **219.57 seconds**. A separately saved **296.133-second** counselor capture contained 24 received STATUS frames and **no BLE MISSING warning**, covering the entire planned update and exceeding the old 120-second grace.
- After closing maintenance and intentionally stopping tag advertisements, normal BLE loss was reported at **age_ms=9836**. QUERY seq=1 received REPLY seq=1; the tag received its ACK on attempt 1.
- SOS seq=7 produced the counselor action and an ACK on attempt 1.
- Restoring tag advertisements produced `RECOVERED tag=T01 seq=59 maintenance=0`.
- Manual QUERY seq=2 overlapped a tag STATUS exchange and did not receive a corresponding reply. An operator retry (seq=3) received REPLY seq=3. This is a remaining reliability limitation, not a loss-free test pass; automatic QUERY retries are not implemented.

Selected final counselor log:

```text
[00:07:48.512,000] ACTION: counselor check tag T01, SOS received
[00:08:00.343,000] BLE MISSING tag=T01 age_ms=9836; requesting LoRa status
[00:08:00.518,000] QUERY tag=T01 seq=1
[00:08:00.658,000] RX tag=T01 type=5 seq=1 rssi=-17 snr=13 motion=UNKNOWN battery=unavailable
[00:08:42.361,000] RECOVERED tag=T01 seq=59 maintenance=0
[00:09:59.490,000] QUERY tag=T01 seq=3
[00:09:59.631,000] RX tag=T01 type=5 seq=3 rssi=-17 snr=13 motion=UNKNOWN battery=unavailable
```

See [wireless-v1115-summary.json](wireless-v1115-summary.json) and [current-builds.json](current-builds.json). Raw captures are `counselor-v1115-grace-only.log`, `counselor-v1115-grace-and-demo.log`, `tag-v1115-sos.log`, `tag-v1115-loss-query.log`, both `*-v1115-verified-images.log` files, and `final-*-status-20261030.log`. Two host presence-decoder tests and both signed role builds passed. The final normal firmware uses hardware SPI and an unmodified pinned Zephyr checkout.
