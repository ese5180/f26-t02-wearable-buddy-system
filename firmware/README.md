# Firmware source map / 固件学习入口

Start with [main.c](src/main.c), then the interfaces under [include/](include/). Each service owns its state; the radio worker alone owns runtime LoRa hardware access. MCUboot is a pinned external workspace dependency, configured under [sysbuild/](sysbuild/) and [conf/](conf/).

先读启动入口，再读头文件接口；各服务管理自己的状态，运行时只有 radio worker 操作 LoRa 硬件。MCUboot 是固定版本的外部工作区依赖，本目录保存它的项目配置。

## Device firmware / 设备固件

| Topic / 内容 | Implementation / 实现 | Learning guide / 学习与复现 |
|---|---|---|
| BLE presence / 心跳 | [presence.c](src/ble/presence.c), [roster.c](src/roster/roster.c) | [中文](../docs/zh/FIRMWARE.md), [English](../docs/en/FIRMWARE.md) |
| LoRa receive/retry / 收发重试 | [link.c](src/radio/link.c), [exchange.c](src/radio/exchange.c), [history.c](src/radio/history.c) | [中文](../docs/zh/FIRMWARE.md#无线帧格式), [English](../docs/en/FIRMWARE.md#on-air-formats) |
| Wire format / 帧格式 | [wire.c](src/protocol/wire.c), [wire.h](include/wire.h) | [中文字段表](../docs/zh/FIRMWARE.md#无线帧格式), [English fields](../docs/en/FIRMWARE.md#on-air-formats) |
| Wio radio hardware / 硬件 | [wio_sx1262.c](src/drivers/wio_sx1262.c) | [Full setup / 完整搭建](../docs/en/GETTING_STARTED.md) |
| IMU / 运动采样 | [imu.c](src/sensors/imu.c), [motion.c](src/sensors/motion.c) | [中文](../docs/zh/IMU_LSM6DSO.md), [English](../docs/en/IMU_LSM6DSO.md) |
| Signed update and confirmation / 签名更新与确认 | [health.c](src/update/health.c), [MCUboot config](sysbuild/mcuboot.conf) | [中文](../docs/zh/FIRMWARE_SECURITY_CHECKOFF.md), [English](../docs/en/FIRMWARE_SECURITY_CHECKOFF.md) |
| Fleet diagnostics / 云端诊断 | [fleet.c](src/fleet/fleet.c), [fleet.conf](conf/fleet.conf) | [操作与指标](../docs/zh/FLEET_MANAGEMENT.md) |
| Wi-Fi cloud OTA / 直连云端更新 | [wifi_fleet.c](src/fleet/wifi_fleet.c), [wifi.conf](conf/wifi.conf) | [中文](../docs/zh/FLEET_MANAGEMENT.md), [English](../docs/en/FLEET_MANAGEMENT.md) |
| USB commands / 开发命令 | [commands.c](src/console/commands.c) | [Host tool / 主机工具](../tools/bg_serial.py) |

## Host commands / 主机命令

| Task / 用途 | Entry point / 入口 | Python environment / 环境 |
| --- | --- | --- |
| Configure, build and sign / 构建签名 | [scripts/build.py](../scripts/build.py) | West workspace |
| Back up and provision a board / 备份和首次安装 | [backup_flash.py](../scripts/backup_flash.py), [flash_initial.py](../scripts/flash_initial.py) | West workspace |
| Install the recovery bootloader / 安装恢复引导 | [install_recovery_bootloader.py](../scripts/install_recovery_bootloader.py) | West workspace |
| Update or inspect the image over BLE / BLE 更新和镜像状态 | [bg_ble.py](../tools/bg_ble.py) | Repository |
| Read logs and send shell commands / 串口日志与命令 | [bg_serial.py](../tools/bg_serial.py) | Repository |
| Set up board Wi-Fi / 配置设备 Wi-Fi | [bg_wifi.py](../tools/bg_wifi.py) | Repository |
| Relay Memfault diagnostics from USB / USB 遥测转发 | [bg_fleet.py](../tools/bg_fleet.py) | Repository |
| Restore an image through MCUboot UART / 串口恢复应用 | [bg_uart_recovery.py](../tools/bg_uart_recovery.py) | Repository, with West workspace `imgtool` for uploads |

The two Python environments are defined in the [setup guide](../docs/en/GETTING_STARTED.md). MCUboot, Zephyr BLE SMP and the Memfault SDK are workspace dependencies. The project's source covers device behavior, build profiles, hardware configuration and host workflows. See [validation](../docs/VALIDATION.md) for the corresponding hardware checks.

两个 Python 环境的用途见[复现指南](../docs/zh/GETTING_STARTED.md)。MCUboot、Zephyr BLE SMP 和 Memfault SDK 由工作区提供；本仓库维护设备逻辑、构建配置、硬件连接和主机工具。
