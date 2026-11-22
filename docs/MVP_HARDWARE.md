# Tag GNSS and vibration / 标签 GNSS 与振动

These opt-in modules run on a BuddyGuard tag. Follow the [Zephyr setup and signed-update guide](en/GETTING_STARTED.md) or its [Chinese version](zh/GETTING_STARTED.md) first.

两块外设都通过构建选项在标签端启用。先按上面的复现指南搭好 Zephyr，并配置这块板信任的签名密钥。

## Wiring / 接线

| Module / 模块 | Tag wiring / 标签接线 | Profile / 选项 |
| --- | --- | --- |
| Seeed L76K | GPS `TX` → XIAO `D7/GPIO44`, `3V3` → `3V3`, `GND` → `GND`. Leave GPS `RX`, `RST`, and `WUP` unconnected. / 只接 TX、3V3 和地。 | `--gnss` |
| DRV2605L + ERM coin motor | `SDA` → `D4`, `SCL` → `D5`, `VIN` → `3V3`, `GND` → `GND`. Connect the two motor leads across `OUT+` and `OUT-`. / 马达两线接驱动板输出。 | `--haptics` |

The motor driver is at I²C address `0x5a` and shares D4/D5 with the LSM6DSO. UART0 is receive-only for the GPS in the application. Disconnect GPS TX before connecting a USB-to-TTL adapter's TX for MCUboot serial recovery.

马达驱动与 IMU 共用 I²C，地址分别是 `0x5a` 和 `0x6a` 或 `0x6b`。通过 UART0 做 MCUboot 串口恢复前，先断开 GPS TX，再接转接器 TX。

## Build and check / 构建和验证

From the repository root, set `BG_WS`, `BG_KEY`, and `TAG_PORT` for your setup. Use a new version for each signed test image and install it using the guide above.

在仓库根目录运行。按自己的环境设置工作区、密钥和 USB 串口变量，签名测试镜像使用独立版本号，烧录按上面的指南操作。

```sh
"$BG_WS/.venv/bin/python" scripts/build.py --workspace "$BG_WS" --key "$BG_KEY" \
  --build-dir "$BG_WS/build-bg-tag1-mvp" --role tag --tag-id 1 \
  --imu --low-power --gnss --haptics --version 1.6.11

.venv/bin/python tools/bg_serial.py --port "$TAG_PORT" command bg gnss
.venv/bin/python tools/bg_serial.py --port "$TAG_PORT" command bg vibrate
```

- Run `bg gnss` twice a few seconds apart. Increasing `rx_bytes` and `valid` show UART reception and checksum-valid NMEA. `NO_FIX` is expected when the receiver has no fix indoors. `FIX_REPORTED` means the receiver reported a fix. `age_ms=-1` means no valid sentence has arrived.
- `bg vibrate` starts one finite effect. `result=0` means the driver accepted it. Feel for one pulse on the bench. Run it again after at least one second.
- 间隔几秒运行两次 `bg gnss`。`rx_bytes` 和 `valid` 持续增加说明已收到并校验 GPS 数据。室内可以用 `NO_FIX` 验证接收。运行 `bg vibrate` 后实际确认马达有一次短振。

## LoRa gather cue / LoRa 集合提示

Build both roles from this revision. Use the tag build above and a separate build directory for the handheld. Send the command over the handheld's USB console:

```sh
"$BG_WS/.venv/bin/python" scripts/build.py --workspace "$BG_WS" --key "$BG_KEY" \
  --build-dir "$BG_WS/build-bg-counselor-mvp" --role counselor --version 1.6.11

.venv/bin/python tools/bg_serial.py --port "$COUNSELOR_PORT" command bg gather 1
```

The counselor sends a unicast GATHER request to T01. T01 starts one short vibration and returns an ACK after the driver accepts the request. Look for `GATHER vibration started` on T01 and `GATHER confirmed` on the counselor. Retransmissions of the same request are ACKed without starting a second vibration. The current LoRa link is for trusted bench use.

两端都用此版本构建，标签端加 `--haptics`。在辅导员端输入 `bg gather 1`，T01 收到后短振一次。看 T01 的 `GATHER vibration started` 和辅导员端的 `GATHER confirmed`。重复收到同一序号时只补发 ACK。可把目标改成已配置的其他标签编号。

The two-board bench tracked one driver start and an ACK on the second LoRa attempt. For a physical check, feel for the short pulse, then repeat with the assembled tag resting still and monitor the IMU for unintended SOS events.

双板已验证一次马达驱动启动和第二次发送后的 LoRa 确认。装壳后用手确认短振，再让标签静置复测，观察 IMU 是否误报 SOS。

## Code to extend / 后续接手

The [L76K driver](../firmware/src/drivers/l76k.c) owns UART0, frame boundaries, checksum validation, and its receive queue. The [GNSS service](../firmware/src/sensors/gnss.c) turns received frames into a status snapshot. The [haptics service](../firmware/src/actuators/haptics.c) selects a fixed effect using Zephyr's DRV2605 driver. The [USB commands](../firmware/src/console/commands.c) call those services.

L76K 驱动负责串口、分帧、校验和入队，GNSS 应用只维护状态。振动应用调用 Zephyr 的 DRV2605 驱动。`radio/link.c` 把集合指令交给同一个振动服务。`bg gather` 目前用于集合提示，坐标上报、摔倒告警联动和穿戴时马达对 IMU 的影响由后续验证覆盖。
