# SparkFun LSM6DSO 接入 / 1.1.17

IMU 从 1.1.17 起接入；1.1.18 辅导员另行关闭扫描器默认去重，以持续接收心跳。两者使用相同的协议版本 1。本次实板为标签 1.1.17＋辅导员 1.1.18；从当前源码新构建则版本为 1.1.18。

[English](../en/IMU_LSM6DSO.md) · [完整操作文档](GETTING_STARTED.md) · [固件解释](FIRMWARE.md)

适用于 **SparkFun SEN-18020、LSM6DSO Qwiic**，不是旧 BOM 的 LSM6DS3TR-C。仅把 IMU 接到儿童标签；辅导员只需要更新软件显示运动状态。

## 接线

断开标签 USB 后接线，检查完成再恢复供电：

| SparkFun | XIAO ESP32S3 标签 |
|---|---|
| 3V3 | 3V3，不能接 5V |
| GND | GND |
| SDA | D4 / GPIO5 |
| SCL | D5 / GPIO6 |

Qwiic 接口可用转杜邦线连接；通孔需要可靠焊接。其他引脚和板上跳线保持默认，不需要 INT 线。该 I2C 接口不占用 Wio LoRa 的 SPI 引脚。

依据：[SparkFun 产品及资料](https://www.sparkfun.com/sparkfun-6-degrees-of-freedom-breakout-lsm6dso-qwiic.html)、[官方硬件说明](https://learn.sparkfun.com/tutorials/qwiic-6dof-lsm6dso-breakout-hookup-guide/hardware-overview)。

## 构建与更新

先按完整操作文档设置 `BG_WS`、`BG_KEY`，本节命令从仓库根目录运行。使用新构建目录，避免与旧 profile 混用：

```sh
"$BG_WS/.venv/bin/python" scripts/build.py --workspace "$BG_WS" --key "$BG_KEY" --build-dir "$BG_WS/build-bg-tag1-imu" --role tag --tag-id 1 --imu
"$BG_WS/.venv/bin/python" scripts/build.py --workspace "$BG_WS" --key "$BG_KEY" --build-dir "$BG_WS/build-bg-counselor-imu" --role counselor
```

`--imu` 仅供标签使用，会启用 `CONFIG_BUDDYGUARD_IMU`、Zephyr sensor/I2C 驱动和 `conf/lsm6dso.overlay`。不带该选项的标签继续发 UNKNOWN；辅导员不需要传感器或 `--imu`。

固件会探测默认 `0x6B` 和备选 `0x6A`，要求寄存器 `WHO_AM_I` (`0x0F`) 返回 `0x6C`；两个地址节点都延迟初始化，仅初始化实际识别到的设备。使用原团队密钥签名并通过 BLE 更新，**已有板子不需要再整片 USB 擦除**。详见主操作文档的签名更新流程，必须为每个角色选择正确镜像。

旧构建目录的 profile 没有 `imu` 字段时，脚本可能拒绝复用；选择新目录即可。不要手改 marker 绕过配置保护。

## 查看数据

设置本次实际端口后：

```sh
.venv/bin/python tools/bg_serial.py --port "$TAG_PORT" command bg imu
.venv/bin/python tools/bg_serial.py --port "$TAG_PORT" --log evidence/raw/my-imu.log monitor --seconds 40
```

`bg imu` 报告 enabled、available、地址、WHOAMI、运动状态、样本数、样本年龄和错误码，以及三轴加速度／三轴角速度。正常应为 `enabled=1 available=1`、WHOAMI `0x6c`、样本数持续增长、年龄明显小于 1000 ms。**available=0 时输出的轴值可能是最后一次有效值，不是当前有效测量。**辅导员或未启用 IMU 的标签显示 `enabled=0`，这是预期行为。

单位：`accel_mg` 的 1000 约为 1g；`gyro_mrad_s` 的 1000 是 1 rad/s。静止时重力仍在加速度中，三轴不应全部要求接近零。陀螺仪可能有小偏置，尚未进行精密校准。

## 采样与状态

`src/sensors/imu.c` 的独立低优先级线程负责所有 IMU I2C 操作；主循环、shell 和 LoRa 只读取 mutex 保护的快照，不在持锁期间做 I2C。

- 传感器 ODR 为 52 Hz，软件每 40 ms 轮询一次（约 25 Hz，实际受调度和 I2C 时间影响）。
- 加速度量程 ±4g，陀螺仪 ±250°/s，使用轮询、不需要中断引脚。
- 读取前检查状态寄存器 `0x1E` 的加速度／陀螺仪新数据位；没有新数据时不刷新时间戳。
- 前 1 秒有效连续采样为 UNKNOWN。超过 1 秒的采样间断需要重新预热。
- 相邻样本加速度三轴变化绝对值之和 >150 mg，或任一轴角速度绝对值 >300 mrad/s，判为检测到活动。
- 活动后保持 MOVING 2 秒；连续没有明显活动后变为 STILL。这些是实验阈值，不是经儿童佩戴数据验证的模型。
- 读取错误立即 UNKNOWN；快照超过 1000 ms 未刷新也 UNKNOWN。缺少传感器不会阻塞无线服务或要求镜像回滚。

纯 C 状态逻辑在 `src/protocol/motion.c`，测试覆盖重力、预热、活动保持、过期和时钟回绕。

## LoRa 演示

帧仍为协议版本 1、12 字节；使用原本预留的第 8 字节：`0=UNKNOWN`、`1=STILL`、`2=MOVING`。STATUS、SOS、QUERY 的 REPLY 在创建时取最新有效状态；重试复用当次快照。其余搭扣、电量字段仍为未知／不可用。原始六轴数据只通过本机日志查看，不放入当前 LoRa 帧。

将 IMU 静置，辅导员应看到周期 `motion=STILL`。轻轻移动／转动 IMU 约 15 秒，覆盖一次约 10 秒的 STATUS 周期，应看到 `motion=MOVING`；放回后约 2 秒本地恢复 STILL，下一次 STATUS 更新辅导员。移动传感器本体，不要拉扯排线或带电拆线。

也可以在运动期间触发标签 `bg sos`，或通过辅导员 `bg query 1` 获取当时状态；先关闭对应端口的串口监视器，避免两个读取程序竞争。队列成功不等于送达，检查接收日志及 ACK。旧 1.1.16 辅导员会硬编码显示 UNKNOWN，需更新辅导员才能正确显示该字段。

STILL 只表示近期未检测到明显运动，不判断孩子是否危险或是否跌倒；IMU 不提供位置或可靠距离。具体实板结果见日期对应的验证记录，不能把编译或模拟输入测试当成真实运动已验证。
