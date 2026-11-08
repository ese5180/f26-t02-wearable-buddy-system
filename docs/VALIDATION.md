# Bench validation / 实板验证摘要

Fleet results below were reported by Siwei Lei in the [source commit](https://github.com/Alex-lei-1/buddyguard/commit/5a730b1823df74a9970945415dc7b4f58a03e83f). Dates, versions and release status describe those sessions.

## Course repository integration

Local builds passed for the Wi-Fi tag (IMU, 60 s metrics), Wi-Fi counselor (15 s metrics and UART recovery) and baseline BLE/LoRa counselor. Both Wi-Fi builds keep the original image slots and validate against the Team 2 public key. The eight upstream host-tool checks passed against the imported tools. Hardware and Memfault results below are the team's earlier bench evidence.

After moving the fleet, update and motion files into their owning directories, the same three build profiles passed again and their images verified with the committed public key. All nine host and build CLI entry points passed `--help`. The firmware C files retained their prior contents.

## A/B cohort rollout, 2026-11-08

A/B adds one build-time SDK interval override and two policy-identifying metrics. BLE presence, LoRa supervision, signed OTA and crash handling are unchanged. The extra-credit demonstration is targeted rollout and a measurable diagnostic policy change, not a statistically powered comparison or battery-life result.

| Check | Observed result |
|---|---|
| Common A baseline | Both devices installed and confirmed 1.5.0 by Wi-Fi; C01 16:05:47 EST cloud report: A_CONTROL, bg_report_interval_s=60, SDK IntervalMs=60000 |
| Cohort assignment | BG-T01 in ab-a-control; BG-C01 in ab-b-fast |
| B-only release | 1.5.1 contains compatible signed tag and counselor payloads and matching symbols; activated only for ab-b-fast |
| Actual targeting | C01 downloaded and confirmed 1.5.1; T01 remained confirmed 1.5.0 despite a compatible B tag payload |
| Natural cadence | C01 B reports captured at 16:07:23 and 16:07:38 EST; decoded IntervalMs=15000, B_FAST and interval=15. T01 A report at 16:06:46 decodes 60000, A_CONTROL and interval=60 |
| Delivery versus collection | Two consecutive B reports arrived together around 16:07:49; upload polling remains about 30 seconds |
| Real device metrics | Both policy reports decode CPU/heap/USB host state and confirmed=1; T01 reports IMU available=1, C01=0 |
| A/B product regression | After controlled BLE loss/recovery, C01 B 1.5.1 natural report at 16:09:24 EST shows bg_missing_events_total=1, bg_lora_rx_total=14 and no LoRa failures/queue drops |
| Current A coredump | T01 1.5.0 deliberate assert rebooted and uploaded successfully; cloud resolves memfault_demo_cli_cmd_assert. Trace 11100250490; initial SNTP timeout delayed upload, automatic retry succeeded |
| Classroom release | 1.5.2 B_FAST signed payloads and matching ELFs uploaded for both roles; deliberately NOT ACTIVATED. This exact version is built and signature-verified, not installed yet |

C01 before/after reports supply a same-device functional comparison. Different tag/counselor workloads cannot support a CPU or power comparison between these two boards. No forced heartbeat commands were used for the cadence samples. Short bench results do not establish field reliability.

All six 1.5.x role/variant builds passed signature verification. Eight host tests passed; the build rejects an interval override without a fleet profile.

The current source VERSION defaults to the A baseline 1.5.0; B builds must use explicit `--heartbeat-seconds 15 --version 1.5.1` (or the next distinct version). Existing bootloaders and runtime credentials were preserved. The exact [live checkoff sequence](zh/FLEET_CHECKOFF.md) and [experiment explanation](zh/FLEET_AB_TEST.md) are included in the project package.

## Direct Wi-Fi fleet validation, 2026-11-08 (earlier 1.4.x run)

Both existing MCUboot installations were retained. The initial Wi-Fi 1.4.0 application was installed using signed BLE OTA; subsequent releases were downloaded by each board directly over AirPennNet-Device using HTTPS, with no host telemetry gateway or BLE downloader running.

| Check | Observed result |
|---|---|
| Direct cloud OTA | T01 and C01 progressed through 1.4.1, 1.4.2 and 1.4.3; final USB status reports actual `version=1.4.3 confirmed=1` on both roles |
| Patched download regression | 1.4.2 includes shared PSRAM heap locking; both devices subsequently installed and confirmed 1.4.3 over Wi-Fi. No new spontaneous crash observed in this short patched run |
| Role/version selection | Cloud payloads are separate for `buddyguard-tag` / `xiao-esp32s3-wio-tag-v1` and counselor equivalents; the SDK sends the running version and device identity |
| Automatic telemetry | Board logs show HTTPS upload result 0 and up-to-date OTA result 0; cloud device records show 1.4.3 |
| Current system/power metrics | C01 1.4.3 report captured 15:37:01 EST decoded CPU 19.14%, Heap_BytesFree 14,740 B, image confirmed 1, USB host active 1 and `USB_HOST_ACTIVE`; these are a snapshot, not fixed expected values |
| Wi-Fi/BLE/LoRa coexistence | On both 1.4.3 roles, SOS was ACKed; C01 observed BLE MISSING at age 9,984 ms, completed LoRa QUERY/REPLY on attempt 1, then RECOVERED. T01 still reports actual LSM6DSO address 0x6b, WHO_AM_I 0x6c and fresh samples |
| Critical product metric over Wi-Fi | After two controlled loss/recovery cycles, latest C01 1.4.3 cloud report decodes `bg_missing_events_total=2`, `bg_lora_rx_total=16`, CPU 9.1%, USB host active 1 and `USB_HOST_ACTIVE` |
| Coredump over Wi-Fi | T01 deliberate assert on 1.4.3 was automatically uploaded and decoded: 20 threads, `shell_uart`, `memfault_demo_cli_cmd_assert`, memfault_demo_panics.c:129. C01 deliberate assert also decoded on 1.4.1 |
| Reboot reasons | C01 cloud distinguishes `Fw Update` for 1.4.2/1.4.3, `User Reset`, `Power On Reset` and deliberate `Assert`; latest requested reset gives hardware 0x0008 and stored intent 0x0002 |
| Earlier prepared target | 1.4.4 remains inactive; superseded by the A/B 1.5.2 classroom release above |
| Host checks | 8 tests pass: existing USB gateway checks plus private Wi-Fi profile validation and pseudo-terminal provisioning/echo handling |

[T01 1.4.3 coredump](https://app.memfault.com/organizations/university-of-pennsylvania-w1vt/projects/nrf-project/issues/1805367483/traces/11100090397?tab=trace-details), [C01 reboot history](https://app.memfault.com/organizations/university-of-pennsylvania-w1vt/projects/nrf-project/devices/BG-C01?tab=reboots), [prepared 1.4.4 release](https://app.memfault.com/organizations/university-of-pennsylvania-w1vt/projects/nrf-project/releases/1.4.4).

During early unpatched 1.4.0/1.4.1 testing, transient HTTPS receive failures retried and spontaneous heap-guard faults were observed. The T01 coredump resolves to `sys_heap_free` through ESP32 Wi-Fi's shared PSRAM allocator. The pinned Zephyr shared multi-heap lacked synchronization, so Wi-Fi builds now apply `patches/zephyr-shared-heap-lock.patch`, serializing runtime allocation/free/realloc while retaining heap statistics. The recorded patch hash is `bc0b9f4f535481c96e69de878f2b3712536ba6136b70099b7266f7db9c4eae49`. This short regression is not proof of long-duration reliability.

Credentials are runtime-provisioned from private local files, stored in NVS, and preserved across OTA; they are absent from the source and build configuration. TLS certificate/hostname validation stays enabled. Firmware is staged in secondary, its tail block flushed, and MCUboot is asked for a test boot only after complete receipt. Existing health confirmation and watchdog/revert logic is retained. No deliberate bad-signature or power-cut experiment was repeated over Wi-Fi in this run; earlier BLE security tests are historical evidence below. HTTP resume is not implemented.

Current build artifact sizes: T01 1.4.3 = 966,924 B signed; C01 = 965,739 B. Corresponding file SHA-256 values: T01 `9aa1010017e6705559e4b023f33d5ae6f83cccecf5ea825378a3228efc68e3a5`, C01 `12e198aabe0c293966c4fa3c77c89fb5807d7c00a8c2e6a7b27989ae5a23cc92`. Build reports verify the team signature and record pinned dependencies and patch identity.

Power telemetry still observes USB host activity only. Battery SoC, VBUS voltage and charging-state measurements are unavailable. Wi-Fi uses 2.4 GHz PSK, not PennKey enterprise authentication. See the [demo sequence](zh/FLEET_CHECKOFF.md).

## Earlier USB gateway validation, 2026-11-08

The 1.3.0 `--fleet` profile passed BLE installation on T01 and C01, real metrics and USB host activity upload, User Reset and Assert decoding, and symbolized coredumps from both roles (25,540 B and 24,732 B). C01 reported a real lost-tag event after BLE loss and LoRa QUERY/REPLY. That run used the USB host gateway with an inactive cloud release.

## Earlier hardware validation

Observed on **2026-10-31**, firmware **1.1.20**, implementation commit `0c3a612` with host-tool fixes through `c94a795`. This is a concise record of earlier hardware work; repository cleanup does not perform another hardware test.

以下为此前实测摘要，本次仓库整理没有重新刷板或重做硬件测试。详细测试源码、日志及旧版本记录保留在维护者本地完整归档与 Git 历史中。

| Check / 项目 | Observed result / 实测结果 |
|---|---|
| Valid BLE FOTA / 正常更新 | T01 and C01 booted 1.1.20 and self-confirmed / 两端启动并自确认 |
| Wrong key / 错误签名 | Full 1.3.2 upload rejected after reset; confirmed 1.1.20 retained |
| Corrupted image / 损坏镜像 | Full 1.4.1 upload rejected; original confirmed digest retained |
| Signed faulty code / 有效签名故障代码 | 1.2.2 booted unconfirmed, WDT reset, reverted to confirmed 1.1.20 |
| Interrupted upload / 中断续传 | Paused at 87,258 B; primary stayed confirmed; same file resumed at that offset |
| BLE loss / 心跳丢失 | MISSING at age 8,619 ms, LoRa QUERY/REPLY, fresh BLE produced RECOVERED |
| Normal operation / 正常运行 | 302 s: 20/20 queries, 8/8 SOS; no unexpected reset/missing/queue drops; retries occurred |
| IMU / 传感器 | LSM6DSO at `0x6b`, WHO_AM_I `0x6c`; real movement/still observation |
| UART extra credit / 串口加分 | Earlier C01 1.1.19: valid signed recovery, wrong-key rejection, recovery without a working application, good-image restoration |

The October 31 BLE run retained existing bootloaders and had no external TTL cable attached. C01 UART recovery was demonstrated earlier the same day. A successful upload alone is not evidence that MCUboot accepted an image.

本轮 BLE 保留原 bootloader，未接外部 TTL；UART 结果来自同日较早的独立测试。上传成功不等于验签通过，必须检查复位后的版本、digest、active 与 confirmed。

## Recorded artifacts / 已记录产物

| Device / 设备 | Unsigned app / 未签名应用 | Signed app / 签名应用 | Installed bootloader / 已装引导 |
|---|---:|---:|---:|
| T01, IMU enabled | 436,948 B | 437,099 B | 46,672 B |
| C01, UART recovery | 435,748 B | 435,899 B | 63,728 B |

Signed-file SHA-256 / 签名文件哈希:

```text
T01 cc5ccdc189f5e492e1972861cd173e8f318617d64c813edcaca886aad54549ca
C01 d6e3a736dd2fffed7b869b1a77f9f4ef9693f9c955fc88264e83287bf2135c87
```

These identify the recorded files; a fresh build can have different hashes and sizes. Each build generates its own `buddyguard-build.json` and verifies its signed image. Private keys, full Flash backups and signed binaries stay outside Git.

以上哈希识别当时产物；重新编译可能改变大小／哈希，以每次生成的构建报告为准。私钥、整片备份及签名二进制不上传。

Not demonstrated: an uninterrupted 30-minute run, outdoor range/power, complete three-tag concurrency, independent teammate-machine reproduction, or deliberate power loss during swap. LoRa delivery is bounded-retry communication, not a guaranteed safety channel.

尚未验证：连续 30 分钟、户外距离／功耗、三标签完整并发、队友另一台电脑独立复现、交换中断电。有限重试不等于保证送达。
