# BuddyGuard MVP firmware handoff / 固件交接

Team 02 · Jet2Holiday · XIAO ESP32S3 + Wio-SX1262 B2B

## Bench status / 实机状态

| Device / 设备 | Verified / 已验证 | Open / 待验证 |
| --- | --- | --- |
| T01, `1.6.12` | LSM6DSO reports motion. Holding Wio USER sends a LoRa SOS to C01. A GATHER starts one short vibration and C01 receives its ACK. / IMU 动静状态、实体按键 SOS、马达短振及 LoRa 确认已实测。 | The soldered L76K sends no readable UART data. / 叠焊的 GPS 尚无 UART 数据。 |
| C01, `1.6.13` | Wio USER sent GATHER to T01. Its own L76K received valid NMEA indoors and reported `NO_FIX`. The background worker uploaded 229 bytes to Memfault over HTTPS. / 实测集合发送、室内 GPS 串口接收和后台遥测上传。 | Counselor phone GATT bridge and a three-tag group test. / 手机 GATT 桥接及三标签联测。 |

T01–T03 use BLE advertisements for presence and LoRa P2P for SOS, queries and GATHER. C01 can use Wi-Fi for fleet management at base. This firmware reports GNSS reception and fix status on the USB console. It does not yet send coordinates or events to the phone.

标签用 BLE 广播心跳，SOS、状态查询和集合通过 LoRa 点对点传输。C01 可通过 Wi-Fi 做设备管理。当前 GPS 通过 USB 命令查看接收和定位状态。坐标和告警到手机的链路需要接手实现。

## Wiring / 硬件接线

| Module / 模块 | Connection / 接线 |
| --- | --- |
| Wio-SX1262 | Seat the underside B2B connector and fit the 915 MHz antenna. Wio USER is active-low GPIO21 and shares the XIAO user LED pin. / 压紧背面 B2B 接头、接 915 MHz 天线。USER 与 XIAO 指示灯共用 GPIO21。 |
| LSM6DSO + DRV2605L on T01 | Share `D4/SDA`, `D5/SCL`, 3.3 V and GND. IMU `0x6B` (alternate `0x6A`), driver `0x5A`. Motor leads go across driver `OUT+` and `OUT-`. / 两块板可以共用 I²C。马达两线接驱动板输出。 |
| L76K on either role | For new assembly, use only GPS `TX` → XIAO `D7/GPIO44`, `3V3` → `3V3` and `GND` → `GND`. Leave GPS `RX`, `WUP` and `RST` open. / 新装板优先用这三根线。 |

GPIO21 is assigned to the button in this build. If using MCUboot UART recovery, disconnect GPS TX before attaching the USB-to-TTL adapter's TX to D7. / GPIO21 由按键占用。做 MCUboot UART 恢复时，先断开 GPS TX，再接 USB 转 TTL 的 TX。

## Build and run / 构建和复现

Use the `codex/mvp-firmware-handoff` branch. Follow the [English setup guide](en/GETTING_STARTED.md) or [中文环境指南](zh/GETTING_STARTED.md) for the pinned Zephyr workspace and host tools. When cloning for the first time, select this branch before running `west init`. Set `BG_WS` to the West workspace and `BG_KEY` to the key it can read outside the repository. Team boards require the existing signing identity or pre-signed artifacts from the team.

先切到交接分支，再按指南安装依赖。已有 Team 02 板子用现有签名身份构建，也可以请队友通过私有渠道提供对应的签名镜像。保留签名私钥在仓库外。以下版本号用于复现实机基线，后续候选镜像使用新版本号。

```sh
git switch codex/mvp-firmware-handoff

"$BG_WS/.venv/bin/python" scripts/build.py --workspace "$BG_WS" --key "$BG_KEY" \
  --build-dir "$BG_WS/build-bg-tag1-mvp" --role tag --tag-id 1 \
  --imu --low-power --gnss --haptics --version 1.6.12

"$BG_WS/.venv/bin/python" scripts/build.py --workspace "$BG_WS" --key "$BG_KEY" \
  --build-dir "$BG_WS/build-bg-counselor-mvp-wifi" --role counselor \
  --gnss --wifi --version 1.6.13

.venv/bin/python -m serial.tools.list_ports -v
```

Use each build's `firmware/zephyr/zephyr.signed.bin` with the [signed BLE update procedure](en/GETTING_STARTED.md#8-signed-update-over-ble). Use separate build directories and IDs `2` or `3` for extra tags. Set `TAG_PORT` and `COUNSELOR_PORT` to the detected ports. If a board is still in ROM download mode, tap the XIAO **R** button with **B** released to return to the installed image.

按[中文 BLE 更新流程](zh/GETTING_STARTED.md)给已装 MCUboot 的板子升级。T02、T03 使用独立构建目录和各自 ID。把两个串口变量设为本机识别到的端口。若 T01 停在原厂下载模式，松开 **B** 并按一下 **R**，即可回到已安装固件。

1. **Identify / 识别。** Run `bg status` on each board. Check role, tag ID, version and `confirmed=1`. On T01, run `bg imu` and `bg vibrate`; observe motion and feel the pulse. / 两端先核对身份，再单测 IMU 和马达。
2. **SOS / 求助。** Wait for C01 to see T01's BLE heartbeat. Monitor both USB consoles and hold the tag's Wio USER button for about 1.5 s. Look for `USER SOS queue result=0` on T01 and `SOS received` on C01. / 先确认 C01 收到 T01 心跳，再长按学生端 USER，观察两个日志。
3. **GATHER / 集合。** Hold C01's Wio USER for about 1.5 s. It queues individual requests for tags seen since boot, including ones whose BLE heartbeats have since gone missing. Confirm one short pulse on T01, `GATHER vibration started` on T01, and `GATHER confirmed` on C01. An ACK means the vibration driver accepted the request. Retransmission of the same request ACKs it again without replaying the vibration. / 辅导员端长按后逐个通知已登记标签。核对实际短振和对应 ACK，重复包不会再振一次。
4. **GNSS / 定位。** Run `bg gnss` twice, a few seconds apart. Increasing `rx_bytes` and `valid` prove communication. An indoor `NO_FIX` is enough for this check. / 两个计数持续增加即通过串口接收检查。室内无定位也可以验证。
5. **Fleet / 云端。** Provision C01 per the [Memfault guide](en/FLEET_MANAGEMENT.md) or [中文云端指南](zh/FLEET_MANAGEMENT.md). Run `mflt test heartbeat` and look for `Wi-Fi telemetry upload: … bytes` with a positive count. / 由设备后台上传，核对日志和云端设备报告。

```sh
.venv/bin/python tools/bg_serial.py --port "$TAG_PORT" command bg status
.venv/bin/python tools/bg_serial.py --port "$COUNSELOR_PORT" monitor --seconds 60
# Single-tag GATHER, with the counselor serial monitor closed:
.venv/bin/python tools/bg_serial.py --port "$COUNSELOR_PORT" command bg gather 1
```

## T01 GPS follow-up / T01 GPS 接手顺序

1. **Pin conflict.** Full-header stacking connects GPS `WUP` to `D0/GPIO1`. The current IMU overlay drives D0 low as an idle Saleae marker, which puts the L76K in standby. Reserve D0 for WUP in a stacked profile and omit that debug marker there. On older Seeed boards, GPS `RST` is on D10, shared with Wio SPI MOSI. On the revised module, RST moved to D2. Check the actual revision before using the full headers. [Seeed change record](https://www.seeedstudio.com/L76K-GNSS-Module-for-Seeed-Studio-XIAO-p-5864.html)
2. **What the bench proved.** T01 received all 33 bytes and parsed one valid test NMEA frame in UART loopback while the IMU was online. The T01 GPS board measured 3.3 V supply and TX, 0 V WUP, and 3.3 V RST. Temporarily driving D0 high brought the measured WUP to 3.3 V but still yielded zero bytes. Two 12-second hardware UART captures in ROM mode, with D0 released and then held high, also saw no bytes or RX edges. The temporary register settings were restored.
3. **Next hardware check.** Probe the GPS TX using its own ground and confirm VCC and reset at the module. If the fully stacked board stays silent with WUP released and RST high, compare it with a known-good L76K on the same three-wire interface, or isolate the current GPS wiring on the bench. Once `rx_bytes` and `valid` advance, verify the module reports a fresh fix outdoors before sending coordinates to the phone.

1. **引脚冲突。** GPS 整排叠焊后，WUP 接 D0。现有 IMU 调试标记把 D0 拉低，GPS 会待机。叠焊版应把 D0 留给 WUP，并关闭这一路调试标记。旧版 L76K 的 RST 在 D10，与 Wio SPI MOSI 共线；新版改到了 D2。接手先核对实物版本。
2. **已排查。** T01 UART 回环收到全部 33 字节并解析出一条有效 NMEA，当时 IMU 在线。GPS 板实测供电和 TX 都是 3.3 V，WUP 是 0 V，RST 是 3.3 V。临时拉高 D0 后，GPS 板的 WUP 实测升到 3.3 V，但仍无数据。进入原厂下载模式后，释放 D0 和拉高 D0 各监听 12 秒，两次均无字节、无 UART 边沿。临时配置已恢复。
3. **下一步。** 先量 GPS 模块侧供电、复位和 TX 波形。若 WUP 已释放、RST 高电平，但还是无 UART 输出，用已知正常的 L76K 三线接法做对照，或在台架隔离这块 GPS。串口通过后再到户外验证实际定位，最后接上坐标转发。

## Code ownership / 模块入口

| Task / 任务 | Source / 文件 |
| --- | --- |
| Wio button and eligible tags / 板载按键和标签列表 | [user_button.c](../firmware/src/input/user_button.c), [roster.c](../firmware/src/roster/roster.c) |
| LoRa messages, retries and vibration / LoRa 消息及振动 | [link.c](../firmware/src/radio/link.c), [exchange.c](../firmware/src/radio/exchange.c), [haptics.c](../firmware/src/actuators/haptics.c) |
| GPS UART driver and application status / GPS 驱动和应用状态 | [l76k.c](../firmware/src/drivers/l76k.c), [gnss.c](../firmware/src/sensors/gnss.c), [l76k.overlay](../firmware/conf/l76k.overlay) |
| D0 debug marker / D0 调试标记 | [lsm6dso.overlay](../firmware/conf/lsm6dso.overlay), [imu.c](../firmware/src/sensors/imu.c) |
| Fleet Wi-Fi and signed OTA / 遥测和签名更新 | [wifi_fleet.c](../firmware/src/fleet/wifi_fleet.c), [health.c](../firmware/src/update/health.c) |

For the phone, agree on the App's BLE interface first. Add the C01 GATT bridge above the roster and LoRa services, then verify T01 SOS and GATHER end to end on a real phone. Keep the existing BLE heartbeat, signed SMP update and Memfault paths as separate checks.

接手机前先对齐 App 的 BLE 接口。C01 从 roster 和 LoRa 服务取状态与事件，再走 GATT 发给手机。先用实机完成 SOS 和集合闭环，再验 GPS 坐标、T02/T03 及原有 BLE 更新和 Memfault。
