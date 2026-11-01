# Firmware source map / 固件学习入口

Start with [main.c](src/main.c), then the interfaces under [include/](include/). Each service owns its state; the radio worker alone owns runtime LoRa hardware access. MCUboot is a pinned external workspace dependency, configured under [sysbuild/](sysbuild/) and [conf/](conf/).

先读启动入口，再读头文件接口；各服务管理自己的状态，运行时只有 radio worker 操作 LoRa 硬件。MCUboot 是固定版本的外部工作区依赖，本目录保存它的项目配置。

| Topic / 内容 | Implementation / 实现 | Learning guide / 学习与复现 |
|---|---|---|
| BLE presence / 心跳 | [presence.c](src/ble/presence.c), [roster.c](src/roster/roster.c) | [中文](../docs/zh/FIRMWARE.md), [English](../docs/en/FIRMWARE.md) |
| LoRa receive/retry / 收发重试 | [link.c](src/radio/link.c), [exchange.c](src/radio/exchange.c), [history.c](src/radio/history.c) | [中文](../docs/zh/WIRELESS_RELIABILITY.md), [English](../docs/en/WIRELESS_RELIABILITY.md) |
| Wire format / 帧格式 | [wire.c](src/protocol/wire.c), [wire.h](include/wire.h) | [Actual C policy tests / 实际C测试](../tests/radio_vectors.c) |
| Wio radio hardware / 硬件 | [wio_sx1262.c](src/drivers/wio_sx1262.c) | [Full setup / 完整搭建](../docs/en/GETTING_STARTED.md) |
| IMU / 运动采样 | [imu.c](src/sensors/imu.c), [motion.c](src/protocol/motion.c) | [中文](../docs/zh/IMU_LSM6DSO.md), [English](../docs/en/IMU_LSM6DSO.md) |
| Firmware health / 更新确认 | [health.c](src/platform/health.c) | [中文](../docs/zh/FIRMWARE_SECURITY_CHECKOFF.md), [English](../docs/en/FIRMWARE_SECURITY_CHECKOFF.md) |
| USB commands / 开发命令 | [commands.c](src/console/commands.c) | [Host tool / 主机工具](../tools/bg_serial.py) |

Keep new sensor access in a sensor/driver module and provide a snapshot. Keep retry policy free of hardware calls so it can be tested on a laptop. Preserve explicit UNKNOWN values when measurements are absent or stale. Code being present does not imply a hardware test passed; consult dated records in [evidence/](../evidence/).

新增传感器放在 sensor/driver 模块并提供状态快照；重试策略不直接调用硬件，便于电脑测试。测量缺失／过期时明确报告 UNKNOWN。代码存在不等于实机通过，实测结果以带日期的 evidence 为准。
