# Next stage: cloud device management

[中文](../zh/NEXT_STEPS.md) · [Current wireless/FOTA procedure](WIRELESS_RELIABILITY.md)

The team confirmed it does not yet have a course Memfault/nRF Cloud project. Cloud integration is not implemented. Current BLE FOTA, LoRa and UART recovery do not depend on a cloud account.

Course section3.11 requires version/OTA, coredumps and at least one critical device datum. Start with counselor C01; keep wearable supervision on BLE/LoRa. Confirm the course organization/project enrollment path and ESP32+Zephyr integration expectations with the teaching team. Keep project credentials outside chat, README and Git.

Proposed module boundaries:

| Planned module | Owns | Acceptance |
|---|---|---|
| `network/wifi` | C01 connection/reconnect/state | Local supervision survives network loss; network reconnects |
| `telemetry/device_info` | Hardware identity, role, software version | Cloud displays the correct device/version |
| `telemetry/metrics` | Unconfirmed radio delivery counts, missing events, reported motion | At least one real critical measurement appears; unknown stays UNKNOWN |
| `platform/crash` | Crash persistence/upload and matching ELF symbols | A controlled crash produces a decoded coredump |
| `update/cloud_fota` | Release checks/download into existing signed secondary flow | Cloud-triggered signed update is confirmed; invalid candidate does not execute |

These are design candidates, not existing source modules. Pin the official SDK and establish a minimal build before choosing storage/interfaces. Crash storage must not overlap MCUboot or either application slot. Retain the original signing identity and unconfirmed trial mechanism. Recheck RAM, Flash, stacks and power when adding networking/TLS/crash persistence.

The [official Memfault Zephyr guide](https://docs.memfault.com/docs/mcu/zephyr-guide) describes a pure-Zephyr MCUboot FOTA backend; its [May2026 changelog](https://docs.memfault.com/changelog/2026/05/31) records ESP32-S3 Zephyr thread decoding support. Actual builds/tests are still required against this project's pinned Zephyr, partition map and XIAO hardware.

Other product work remains: child-to-child pairing, three-tag concurrency, clasp/motor/battery/GNSS, management authorization, authenticated business frames and replay protection. BLE contact loss is not a location/distance estimate. Signed firmware does not make supervision data confidential.

Other course delivery work remains: independent teammate clone/build/flash verification; Zephyr Ztest/HIL and PPK2 power measurements; Git hooks and a reproducible CI build. The current host tests are not a completed Zephyr Ztest/CI setup.
