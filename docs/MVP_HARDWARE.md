# BuddyGuard MVP firmware handoff / 固件交接

Team 02 · Jet2Holiday · XIAO ESP32S3 + Wio-SX1262 B2B

## Bench status / 实机状态

| Device / 设备 | Verified / 已验证 | Open / 待验证 |
| --- | --- | --- |
| T01, `1.6.16` | L76K acquired an outdoor fix and T01 replied to one LoRa location request with a valid position. LSM6DSO motion, button SOS and GATHER short vibration were verified on the bench. / 实测户外定位、点名坐标回传、IMU 动静状态、按键 SOS 和集合短振。 | Repeat GPS cold starts and test the enclosed, battery-powered tag. / 复测 GPS 冷启动、装壳和电池供电。 |
| C01, `1.6.16` | GATT `SYNC`, one-tag LoRa roll call and forwarding `TAG_POSITION` to the live Web client were verified. C01 also reported its own GPS fix and uploaded telemetry to Memfault. / 实测 BLE 同步、单标签点名、网页位置更新、C01 自身定位和云端遥测。 | Three-tag roll call and signed Wi-Fi OTA regression on the final build. / 三标签联测及最终固件的 Wi-Fi 签名更新回归。 |

Tags broadcast BLE presence. SOS, queries, GATHER and on-demand GPS use LoRa P2P. The browser sends commands and receives events over a direct BLE connection to C01. Both roles use Wi-Fi at base for Memfault diagnostics and signed OTA. See the [Web roll-call contract](WEB_ROLL_CALL.md).

标签用 BLE 广播心跳。SOS、状态查询、集合和点名位置走 LoRa。网页直接通过 BLE 与 C01 收发，两种设备都在基地用 Wi-Fi 做设备管理和签名 OTA。

## Wiring / 硬件接线

| Module / 模块 | Connection / 接线 |
| --- | --- |
| Wio-SX1262 | Seat the underside B2B connector and fit the 915 MHz antenna. Wio USER is active-low GPIO21 and shares the XIAO user LED pin. / 压紧背面 B2B 接头、接 915 MHz 天线。USER 与 XIAO 指示灯共用 GPIO21。 |
| LSM6DSO + DRV2605L on T01 | Share `D4/SDA`, `D5/SCL`, 3.3 V and GND. IMU `0x6B` (alternate `0x6A`), driver `0x5A`. Motor leads go across driver `OUT+` and `OUT-`. / 两块板可以共用 I²C。马达两线接驱动板输出。 |
| L76K on either role | For new assembly, use only GPS `TX` → XIAO `D7/GPIO44`, `3V3` → `3V3` and `GND` → `GND`. Leave GPS `RX`, `WUP` and `RST` open. / 新装板优先用这三根线。 |

GPIO21 is assigned to the button in this build. If using MCUboot UART recovery, disconnect GPS TX before attaching the USB-to-TTL adapter's TX to D7. / GPIO21 由按键占用。做 MCUboot UART 恢复时，先断开 GPS TX，再接 USB 转 TTL 的 TX。

## Build and run / 构建和复现

Use the `codex/mvp-firmware-handoff` branch. Follow the [English setup guide](en/GETTING_STARTED.md) or [中文环境指南](zh/GETTING_STARTED.md) for the pinned Zephyr workspace and host tools. When cloning for the first time, select this branch before running `west init`. Set `BG_WS` to the West workspace and `BG_KEY` to the key it can read outside the repository. Team boards require the existing signing identity or pre-signed artifacts from the team.

先切到交接分支，再按指南安装依赖。已有 Team 02 板子用现有签名身份构建，也可以请队友通过私有渠道提供对应的签名镜像。保留签名私钥在仓库外。先把 `BG_T01_VERSION` 和 `BG_C01_VERSION` 设成准备发布的新版本号。

```sh
git switch codex/mvp-firmware-handoff

"$BG_WS/.venv/bin/python" scripts/build.py --workspace "$BG_WS" --key "$BG_KEY" \
  --build-dir "$BG_WS/build-bg-tag1-rollcall" --role tag --tag-id 1 \
  --imu --low-power --gnss --haptics --tag-position --wifi --version "$BG_T01_VERSION"

"$BG_WS/.venv/bin/python" scripts/build.py --workspace "$BG_WS" --key "$BG_KEY" \
  --build-dir "$BG_WS/build-bg-counselor-rollcall" --role counselor \
  --gnss --wifi --version "$BG_C01_VERSION"

.venv/bin/python -m serial.tools.list_ports -v
```

Use separate build directories and IDs `2` and `3` for T02 and T03. Set `TAG_PORT` and `COUNSELOR_PORT` to the detected ports. Use the [USB provisioning procedure](en/GETTING_STARTED.md#6-first-usb-provisioning--new-boards-only) for a new board. The historical T01 MVP build omitted `--wifi`. A board still on that build needs a Wi-Fi-enabled signed baseline installed and verified before its first cloud update. Back up the board before any USB re-provisioning.

T02、T03 使用独立构建目录和各自 ID。新板先经 USB 安装签名基线。若标签仍在未启用 Wi-Fi 的旧构建，先备份并装好带 `--wifi` 的签名基线，再配置无线网络和 Memfault。已有可用 Wi-Fi 基线的板可直接走下面的云端更新流程。

1. **Identify / 识别。** Run `bg status` on each board. Check role, tag ID, version and `confirmed=1`. On T01, run `bg imu` and `bg vibrate`; observe motion and feel the pulse. / 两端先核对身份，再单测 IMU 和马达。
2. **SOS / 求助。** Wait for C01 to see T01's BLE heartbeat. Monitor both USB consoles and hold the tag's Wio USER button for about 1.5 s. Look for `USER SOS queue result=0` on T01 and `SOS received` on C01. / 先确认 C01 收到 T01 心跳，再长按学生端 USER，观察两个日志。
3. **GATHER / 集合。** Hold C01's Wio USER for about 1.5 s. It queues individual requests for tags seen since boot, including ones whose BLE heartbeats have since gone missing. Confirm one short pulse on T01, `GATHER vibration started` on T01, and `GATHER confirmed` on C01. An ACK means the vibration driver accepted the request. Retransmission of the same request ACKs it again without replaying the vibration. / 辅导员端长按后逐个通知已登记标签。核对实际短振和对应 ACK，重复包不会再振一次。
4. **GNSS / 定位。** Run `bg gnss` twice, a few seconds apart. Increasing `rx_bytes` and `valid` prove UART reception. Wait for `FIX_REPORTED` on T01 before testing the map. / 计数增加可验证串口。地图实测前确认 T01 显示 `FIX_REPORTED`。
5. **Roll call / 点名。** Connect the Web client to `BG-C01` and wait until T01 appears. Click Roll call once and check the last-location time. For a USB-only trigger run `bg locate` on C01 and look for `LOCATE complete tag=T01 ... fix=1`. / 网页连接后点一次名，核对最后定位时间。串口 `bg locate` 可独立触发同一条 LoRa 路径。
6. **Fleet / 云端。** Provision C01 per the [Memfault guide](en/FLEET_MANAGEMENT.md) or [中文云端指南](zh/FLEET_MANAGEMENT.md). Run `mflt test heartbeat` and look for `Wi-Fi telemetry upload: … bytes` with a positive count. / 由设备后台上传，核对日志和云端设备报告。

```sh
.venv/bin/python tools/bg_serial.py --port "$TAG_PORT" command bg status
.venv/bin/python tools/bg_serial.py --port "$COUNSELOR_PORT" monitor --seconds 60
# Single-tag GATHER, with the counselor serial monitor closed:
.venv/bin/python tools/bg_serial.py --port "$COUNSELOR_PORT" command bg gather 1
# Roll call, with the counselor serial monitor closed:
.venv/bin/python tools/bg_serial.py --port "$COUNSELOR_PORT" command bg locate
```

## Wi-Fi OTA / 基地更新

1. **Provision / 配置。** Verify `bg status` reports the intended device and `confirmed=1`. Configure each device's 2.4 GHz Wi-Fi and Memfault access with the [Fleet provisioning guide](en/FLEET_MANAGEMENT.md#2-install-and-provision) or [中文配置指南](zh/FLEET_MANAGEMENT.md). / 先核对设备和确认状态，再在基地为每台配置 Wi-Fi 和 Memfault。
2. **Target / 定向发布。** Upload the role's `firmware/zephyr/zephyr.signed.bin` and matching ELF to Memfault. Tag IDs are compiled into the images. Give each tag image a distinct release version and target only the matching device using its cohort. / 标签 ID 写在镜像里，每枚标签使用独立发布版本和对应 cohort，避免更新后多枚标签变成同一个 ID。
3. **Install / 更新。** For C01, disconnect the Web BLE session before requesting `bg_wifi check`. The current firmware defers the download while a BLE client is connected. Wait for Wi-Fi OTA progress and reboot. / C01 先断开网页 BLE，再用 `bg_wifi check` 触发检查，看下载进度和重启。
4. **Verify / 验证。** Check the new version, device ID and `confirmed=1` via `bg status` and check the updated version in Memfault. Reconnect the Web client on C01 after confirmation. / 核对新版、设备 ID 与确认状态，再连回网页。

## T01 GPS follow-up / T01 GPS 接手顺序

1. **Stacked boards.** Full-header mounting connects GPS `WUP` to D0. The `--tag-position` overlay keeps D0 pulled up as an input and removes the IMU's Saleae marker from that pin. Older Seeed modules route GPS `RST` to D10, which shares Wio SPI MOSI. The revised module routes it to D2. Check your actual board revision. / 叠焊时 D0 连接 WUP，新配置用上拉输入并停用调试脉冲。旧版 RST 在 D10，新版在 D2。[Seeed change record](https://www.seeedstudio.com/L76K-GNSS-Module-for-Seeed-Studio-XIAO-p-5864.html)
2. **Continuous bad checksums.** During the bench test, raw bytes arrived but the `invalid` count rose continuously. A full T01 power cycle, with USB and battery power removed, restored valid NMEA. The tag then obtained a fix and returned it in a roll call. Repeat this on fully assembled tags and record occurrences. / 若字节增加但校验持续失败，台架上完全断电后恢复了有效报文并成功回传定位。装壳后要复测故障频率。
3. **Map not updating.** If T01 shows `FIX_REPORTED`, watch C01 for a LoRa LOCATE after the click. If `bg locate` updates the map and the button triggers no LOCATE, check the Web Control write for `op=3` and a fresh request ID. / 串口触发能更新但网页按钮没有发定位请求时，核对请求格式和请求号。

## Code ownership / 模块入口

| Task / 任务 | Source / 文件 |
| --- | --- |
| Wio button and eligible tags / 板载按键和标签列表 | [user_button.c](../firmware/src/input/user_button.c), [roster.c](../firmware/src/roster/roster.c) |
| LoRa messages, retries and vibration / LoRa 消息及振动 | [link.c](../firmware/src/radio/link.c), [exchange.c](../firmware/src/radio/exchange.c), [haptics.c](../firmware/src/actuators/haptics.c) |
| GPS UART driver and application status / GPS 驱动和应用状态 | [l76k.c](../firmware/src/drivers/l76k.c), [gnss.c](../firmware/src/sensors/gnss.c), [l76k.overlay](../firmware/conf/l76k.overlay) |
| Web GATT and location forwarding / 网页 GATT 与定位转发 | [app_ble.c](../firmware/src/ble/app_ble.c), [wire.c](../firmware/src/protocol/wire.c) |
| D0 marker and GPS wake-up / D0 调试标记和 GPS 唤醒 | [lsm6dso.overlay](../firmware/conf/lsm6dso.overlay), [l76k-position.overlay](../firmware/conf/l76k-position.overlay) |
| Fleet Wi-Fi and signed OTA / 遥测和签名更新 | [wifi_fleet.c](../firmware/src/fleet/wifi_fleet.c), [health.c](../firmware/src/update/health.c) |

See [next steps in the project README](../README.md#next-steps--后续工作) for multi-tag testing, the Web UI's no-fix and no-reply states, and update regression. / 后续任务见项目 README。
