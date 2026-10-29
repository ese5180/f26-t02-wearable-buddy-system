# Hardware validation — 29 October 2026

These are observations from two physical XIAO ESP32S3 + Wio-SX1262 B2B kits on the development Mac. Each board was backed up and provisioned once over USB. Subsequent firmware changes were transferred over BLE SMP; USB supplied power and logs. Private keys, full flash backups and raw captures are excluded from Git.

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
