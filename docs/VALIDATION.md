# Bench validation / 实板验证摘要

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

The latest BLE run retained existing bootloaders and had no external TTL cable attached. C01 UART recovery was demonstrated earlier the same day. A successful upload alone is not evidence that MCUboot accepted an image.

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

Not demonstrated: an uninterrupted 30-minute run, outdoor range/power, complete three-tag concurrency, independent teammate-machine reproduction, deliberate power loss during swap, or cloud integration. LoRa delivery is bounded-retry communication, not a guaranteed safety channel.

尚未验证：连续 30 分钟、户外距离／功耗、三标签完整并发、队友另一台电脑独立复现、交换中断电及云集成。有限重试不等于保证送达。
