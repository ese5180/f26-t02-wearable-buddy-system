# Firmware source map / 固件学习入口

Start with [main.c](src/main.c), then the interfaces under [include/](include/). Each service owns its state; the radio worker alone owns runtime LoRa hardware access. MCUboot is a pinned external workspace dependency, configured under [sysbuild/](sysbuild/) and [conf/](conf/).

先读启动入口，再读头文件接口；各服务管理自己的状态，运行时只有 radio worker 操作 LoRa 硬件。MCUboot 是固定版本的外部工作区依赖，本目录保存它的项目配置。

| Topic / 内容 | Implementation / 实现 | Learning guide / 学习与复现 |
|---|---|---|
| BLE presence / 心跳 | [presence.c](src/ble/presence.c), [roster.c](src/roster/roster.c) | [中文](../docs/zh/FIRMWARE.md), [English](../docs/en/FIRMWARE.md) |
| LoRa receive/retry / 收发重试 | [link.c](src/radio/link.c), [exchange.c](src/radio/exchange.c), [history.c](src/radio/history.c) | [中文](../docs/zh/FIRMWARE.md#无线帧格式), [English](../docs/en/FIRMWARE.md#on-air-formats) |
| Wire format / 帧格式 | [wire.c](src/protocol/wire.c), [wire.h](include/wire.h) | [中文字段表](../docs/zh/FIRMWARE.md#无线帧格式), [English fields](../docs/en/FIRMWARE.md#on-air-formats) |
| Wio radio hardware / 硬件 | [wio_sx1262.c](src/drivers/wio_sx1262.c) | [Full setup / 完整搭建](../docs/en/GETTING_STARTED.md) |
| IMU / 运动采样 | [imu.c](src/sensors/imu.c), [motion.c](src/protocol/motion.c) | [中文](../docs/zh/IMU_LSM6DSO.md), [English](../docs/en/IMU_LSM6DSO.md) |
| Firmware health / 更新确认 | [health.c](src/platform/health.c) | [中文](../docs/zh/FIRMWARE_SECURITY_CHECKOFF.md), [English](../docs/en/FIRMWARE_SECURITY_CHECKOFF.md) |
| USB commands / 开发命令 | [commands.c](src/console/commands.c) | [Host tool / 主机工具](../tools/bg_serial.py) |

Read in three passes: **main + public headers → service implementation → driver/Zephyr internals as needed**. Module headers explain ownership; public API comments explain caller constraints; inline comments explain non-obvious timing, lifecycle and safety decisions. Straightforward assignments do not need narration. MCUboot source is fetched by west into `bootloader/mcuboot/` in the workspace, rather than copied into this repository.

分三遍读：**main＋头文件 → 服务实现 → 按需看驱动／Zephyr 内部**。文件头说明归属，接口注释说明调用限制，行内注释解释不直观的时序、生命周期和安全原因；普通赋值不逐行翻译。MCUboot 源码由 west 下载至工作区的 `bootloader/mcuboot/`，本仓库保留配置与必要补丁。

Keep sensor access in a sensor/driver module and expose a snapshot; keep retry policy independent of hardware. Missing/stale measurements must remain UNKNOWN. See the [validation summary](../docs/VALIDATION.md) for observed hardware results.

新增传感器继续放在 sensor/driver 模块并提供快照；重试策略不直接调用硬件。测量缺失／过期必须保留 UNKNOWN；实测范围见[验证摘要](../docs/VALIDATION.md)。
