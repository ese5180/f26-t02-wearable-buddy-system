# MCUboot UART recovery hardware validation — 2026-10-31

[English runbook](../docs/en/UART_RECOVERY.md) · [中文操作流程](../docs/zh/UART_RECOVERY.md) · [Structured results / 结构化结果](uart-recovery-20261031-summary.json) · [Selected logs / 精选日志](uart-recovery-20261031.log)

**Result / 结果：C01 passed valid signed UART upload, wrong-key rejection, no-application recovery and restoration to1.1.19. / C01 已通过正确签名上传、错误签名拒绝、无应用恢复，并恢复到1.1.19。** T01 was not connected during this run; its earlier IMU/LoRa validation remains separately dated.

| Experiment / 测试 | Observed outcome / 实测 |
|---|---|
| Current full backup / 当前整片备份 |8 MiB read and SHA-256 verified before changes; kept outside Git / 修改前整片备份并校验，保存在 Git 外 |
| Bootloader-only update / 只改引导程序 |63,728-byte MCUboot installed at0x0; full readback matched new bootloader and preserved every byte at/above 0x10000 / 整片读回确认应用及其余区域未变 |
| UART cabling / 串口接线 |CP2102,115200 8N1; TX→D7/GPIO44, RX←D6/GPIO43, common GND, red VCC disconnected; XIAO USB powered / 共地、板子独立 USB 供电 |
| Valid signature / 有效签名 |435,564-byte1.1.19 uploaded to primary over MCUboot UART, reset, actual COUNSELOR1.1.19 startup and confirmed1 / 上传后实际启动 |
| Wrong root / 错误密钥 |1.3.1 validates under its own test root, fails team-root verification; all435,563 bytes uploaded; MCUboot's validated list omitted primary / 完整上传，但验证排除 primary |
| Rejection at reset / 启动拒绝 |No1.3.1 application banner or application jump after reset; MCUboot answered about 33 seconds later, listing only valid secondary 1.1.17 / 超过入口窗口后仍停留在恢复服务 |
| Restore without application / 无应用恢复 |Good 1.1.19 uploaded again through UART, with no functioning application/BLE needed / 不依赖应用或 BLE 重新恢复 |
| Final application / 最终应用 |COUNSELOR1.1.19, BLE slot0 active/confirmed with expected digest, SX1262 SPI readback0x1424, normal advertising after BLE disconnect; maintenance closed / 版本、BLE、射频初始化、维护窗口状态已核对 |
| Reproducible build / 复现构建 |A second fresh-directory signed recovery build passed; bootloader again63,728 bytes / 新目录构建验证通过 |
| Offline guards / 离线检查 |12 tests passed, including backup/root/size/header guards and reset-window probing / 备份、密钥、大小、镜像头及入口探测测试通过 |

## Diagnosis and implemented correction / 定位与修复

The60,624-byte feasibility build received UART bytes in its hardware FIFO but did not service the interrupt-driven receive adapter in the pinned Xtensa single-thread configuration. Enabling the hidden `MULTITHREADING` symbol from a config fragment alone was ineffective. A one-line first-default MCUboot Kconfig patch enables it for ESP32 serial recovery. `build.py --serial-recovery` applies the patch idempotently and checks the generated configuration and64 KiB size limit. Temporary diagnostic changes were removed; the workspace MCUboot diff contains only that tracked prerequisite. The Zephyr checkout itself remains unmodified.

原可行性版本虽然收到 UART 数据，但单线程配置未处理接收中断。现在使用一条受版本控制的 MCUboot Kconfig 默认值修复，由脚本自动应用，并检查最终线程配置与分区容量。临时诊断修改已移除，没有修改 Zephyr 本体。

The host also needed two distinct timeouts: repeat a short parameters probe while waiting for reset, accepting a parsed `ENOTSUP` as proof that MCUboot is present; only then issue the slower15-second slot-hash query. An earlier attempt missed the entry window because an unanswered fast probe was followed immediately by a long image-list wait. Those connection timeouts occurred before upload and are **not** counted as signature rejection.

电脑端采用“快速探测入口→确认收到回复→较长镜像查询超时”。之前连接窗口错过时尚未写入应用，不把这些连接超时算作拒签证据。

## Exact artifacts / 实测产物

- Team public-key SHA-256: `243b7509cec77ffaa1167f1d817836e3a5f3bfc3e3627b15dba457003eecba28`.
- Installed UART MCUboot: `0fa4e54cc987f563ada36c5cf819285d9e0babd9a8ad7cc1fb50c576a139743e`.
- Good 1.1.19 signed file: `23b5a87054a3743ce012d37a64a6d096fd206e24474516b9d38cd8b851dee37a`.
- Good image digest, also returned by BLE: `63b9ba9bf1f0b1e236081bf047e89d068c9b8668dd52691bcded4ed8c11ab8fe`.
- Wrong-root1.3.1 signed file: `b7e5f903124b2700f313bf51894e5077e02804e86aa3998f7e7b33c379f29109`.

Fresh builds can differ due to timestamps and ECDSA signatures; compare each artifact against its own report. The exact tested binaries are preserved locally under ignored `artifacts/uart-recovery-20261031/`, not uploaded to Git.

## Interpretation and limits / 结论边界

This is MCUboot-level UART recovery, not ESP ROM/esptool flashing. Recovery writes primary and can leave no bootable application; the no-application server made restoration possible. The recovered good image booted already confirmed; no new BLE-style trial/revert behavior is claimed. The wrong-key case proves failure to execute plus recovery availability, not automatic rollback to the previous primary.

本次证明了 MCUboot 层串口恢复与错误签名不执行，不把 UART 覆盖 primary 描述为 BLE 试运行／自动回滚。恢复后的有效镜像已确认，不宣称又验证了一次延迟自确认。

Only C01 and the adapter were connected. LoRa chip initialization passed, but two-device packet exchange and IMU sampling were not re-tested. The adapter's logic voltage was not independently measured; reproduction requires a documented3.3V UART adapter. Physical access authentication, hardware secure-boot eFuses, Flash encryption and anti-downgrade remain unimplemented. Original backup and all private keys remain outside Git.
