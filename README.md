# BuddyGuard

**Team Number: 02**

**Team Name: Jet2Holiday**

| Team Member Name | Email Address |
| --- | --- |
| Siwei Lei | <laialex@engineering.upenn.edu> |
| Rico Zhuang | <zzhuan13@engineering.upenn.edu> |
| Yunzhe Deng | <deng1@engineering.upenn.edu> |
| Yubin Guan | <guan1@engineering.upenn.edu> |

**GitHub Repository URL:** <https://github.com/ese5180/f26-t02-wearable-buddy-system>

BuddyGuard helps camp counselors notice when a camper falls behind or may need help. Campers wear screen-free tags. The counselor carries C01, which scans the tags' BLE heartbeats, uses LoRa P2P for field messages, and connects locally to the counselor's web interface over BLE. C01 can use Wi-Fi at base for device diagnostics and signed updates. See the [refined concept](docs/CONCEPT.md).

## Start here

- [Concept and rental model](docs/CONCEPT.md) distinguishes the tested bench demo, the paid pilot kit and the commercial group kit.
- [Project stages and deliverables](docs/PROJECT_LOG.md) covers sections 3.1 through 3.13 and links to the available work.
- [Documentation index](docs/README.md) lists the English and Chinese setup, firmware and demo guides.
- [Firmware source map](firmware/README.md) links each feature to its code and build configuration.
- [Bench validation](docs/VALIDATION.md) records the exact versions and tests for one tag and one counselor.

## Current MVP · Firmware 1.6.16

The live demo uses one student tag (T01) and one counselor gateway (C01). Both boards booted signed `1.6.16` images and reported `confirmed=1`.

- **Routine check-in.** Tags already broadcast BLE heartbeats. C01 reports online, missing and recovered states. When a heartbeat goes missing, C01 asks over LoRa for tag status. The status frames carry motion information. / 平时签到已经自动运行。C01 按 BLE 心跳判断在线、失联和恢复，失联后用 LoRa 查询标签状态。
- **Roll-call GPS.** The browser requests a roll call over BLE. C01 then asks each tag it has seen since boot for one recent valid fix over LoRa and forwards each accepted fix as a BLE `TAG_POSITION` event. The two-board test returned a valid T01 fix, and the team confirmed the Web map updates. A tag with no fresh fix leaves its previous map position and timestamp in place. / 网页点名的 GPS 回传已实现。点一次名，标签通过 LoRa 回一次有效定位，由 C01 转给网页。[Web integration](docs/WEB_ROLL_CALL.md)
- **IMU, SOS and gather.** T01 reports moving or still. A strong impact currently triggers the same SOS path as the Wio USER button. The bench tests verified button SOS and a GATHER request that started one short vibration with a LoRa ACK. / 当前 IMU 能判断动静和撞击，撞击会触发 SOS。摔倒识别是下一阶段。
- **GPS and fleet management.** Both boards obtained GPS fixes outdoors. C01's Wi-Fi telemetry upload to Memfault was verified independently of the Web BLE session.

To reproduce the demo, build both roles using the [MVP handoff](docs/MVP_HARDWARE.md). Open the team's Web client in live BLE mode, connect to `BG-C01`, wait for T01 to appear online, and tap **Roll call** when the tag has a fresh fix. Look for a new **last-location time** even if the marker stays in the same place. The C01 USB command `bg locate` exercises the same LoRa path.

复现时先确认 T01 在线、有有效定位，再点一次名，检查网页最后定位时间是否刷新。

### Location triggers / 定位发送条件

GPS reports are event-triggered. The tag sends a position over LoRa to C01, and C01 forwards it to the Web client over BLE. Routine BLE check-ins report presence. / 平时用 BLE 心跳签到。以下四种事件触发 GPS，通过 LoRa 回传给 C01，再由 C01 通过 BLE 更新网页。

| Trigger / 触发事件 | Expected behavior / 行为 | Status / 进度 |
| --- | --- | --- |
| Possible fall / 疑似摔倒 | The tag sends the fall alert and its available GPS position. / 标签发出疑似摔倒告警，并回传可用位置。 | Fall classification and GPS reporting pending. / 摔倒判断与定位联动待做。 |
| Missing check-in / 丢签到 | When BLE heartbeats time out, C01 requests the missing tag's position over LoRa. / BLE 心跳超时后，C01 用 LoRa 向对应标签查位置。 | LoRa status query is available. GPS request pending. / 已能查状态，查位置待接。 |
| Web roll call / 网页点名 | C01 asks each enrolled tag for one position per roll call. / C01 逐个向已登记标签查一次位置。 | Verified with T01 and C01. / 单标签实测通过。 |
| Manual SOS / 手动 SOS | The tag sends the SOS and its available GPS position. / 标签发出手动 SOS，并回传可用位置。 | Button SOS verified. GPS reporting pending. / 按键 SOS 已实测，定位联动待做。 |

Send the alert promptly while obtaining location. When a fresh fix or LoRa reply is unavailable, keep the last received location and timestamp visible and show the outcome. / 告警先发，定位有结果再更新。无新定位或无回包时，网页保留最近位置和时间，并显示本次结果。

**Planned C01 vibration alert / 辅导员端振动提醒（待做）。** Add a vibration motor to C01. Trigger a short local vibration when C01 receives a new manual SOS or possible-fall alert, or when it first marks a tag missing. The alert works while the browser is disconnected and while a position is pending. Count repeated packets and an ongoing missing state as the same alert. / 辅导员端加一个马达。收到新的手动 SOS、疑似摔倒，或首次判定某枚标签丢签到时短振。网页断开或定位未返回时，C01 仍能提醒。同一事件的重发和持续失联只提醒一次。

### Next steps / 后续工作

Complete the [3.11 MVP checkoff checklist](docs/MVP_CHECKOFF.md) before marking the demo ready. The checklist maps the implementation and bench tests to HRS/SRS and records the submission steps. / 按 [3.11 完成与验收清单](docs/MVP_CHECKOFF.md) 联调、实测和提交。

- Provision T02 and T03, run the 30-minute three-tag offline test, and verify LoRa when a tag stops checking in or two tags send SOS during roll call. / 完成三标签离线联测，覆盖丢签到和双标签并发 SOS。
- Add a distinct possible-fall alert, C01 haptics and incident GPS. Verify no-fix handling and browser disconnect/reconnect. / 补独立摔倒告警、辅导员短振和事件定位，复测无定位及网页重连。
- Verify the final C01 BLE OTA and Memfault flow. Publish the matching firmware, Web entry point and README, then submit the README PDF to Gradescope before checkoff. / 复测 OTA 与云端，更新 GitHub 后从同一版 README 导出 PDF 提交。

## 3.6 Fleet Management submission

[English README](docs/submissions/3.6-fleet-management/README.md) · [PDF export](docs/submissions/3.6-fleet-management/README.pdf)

The submission covers Memfault version reporting, signed Wi-Fi OTA, coredumps, reboot reasons, Zephyr and BuddyGuard metrics, USB host activity, and the 60 s versus 15 s A/B rollout. Start with the [English Fleet guide](docs/en/FLEET_MANAGEMENT.md) or [中文操作指南](docs/zh/FLEET_MANAGEMENT.md) to reproduce it.

## 3.7 Closing the Loop submission

[Closed-loop README](docs/closed-loop/README.md) · [Impact pass test](docs/closed-loop/impact-pass-test.md) · [Low-power pass test](docs/closed-loop/lowpower-pass-test.md)

The team used [closed-loop-starter](https://github.com/SolidPD/closed-loop-starter) to define pass conditions, build, flash and measure two tag features. **Impact → automatic SOS** (`--imu`) queues a LoRa SOS after a sudden acceleration change of 1000 mg or more. The standalone IMU bench build pulses D0 for the Saleae. The GPS roll-call build reserves D0 for L76K wake-up. **Low-power profile** (`--low-power`) sets an 80 MHz CPU clock and was measured against the 240 MHz build with the PPK2.

## Build and run

Follow [Getting started](docs/en/GETTING_STARTED.md) or [中文复现](docs/zh/GETTING_STARTED.md) to set up native macOS, Python 3.12, west 1.5.0 and Zephyr SDK 1.0.1. The board target is `xiao_esp32s3/esp32s3/procpu`, with dependencies pinned in [west.yml](west.yml).

| Build profile | What it enables |
| --- | --- |
| `--role tag` / `--role counselor` | BLE presence and signed BLE SMP updates, LoRa P2P and role-specific application behavior |
| `--imu` | Optional LSM6DSO on a tag, with impact → automatic SOS |
| `--gnss` | L76K receive and GPS fix status on either role |
| `--tag-position` | Tag only, with `--gnss`. Replies to LoRa roll-call requests with a recent GPS fix |
| `--haptics` | Tag only. Enables the DRV2605L vibration driver for GATHER |
| `--wifi` | Memfault diagnostics and signed HTTPS OTA |
| `--fleet` | Memfault diagnostics with a USB host gateway when used without `--wifi` |
| `--serial-recovery` | Optional MCUboot recovery over UART |
| `--low-power` | Closed-loop low-power profile: 80 MHz CPU |

Build and sign with [scripts/build.py](scripts/build.py). Existing team boards use the Team 2 signing identity. Runtime Wi-Fi credentials and private signing keys are kept outside the repository. See the setup guides before provisioning a board.

## Repository layout

| Path | Contents |
| --- | --- |
| [firmware/](firmware/) | Zephyr application, board configuration and MCUboot configuration |
| [scripts/](scripts/) | Build, flash backup, initial provisioning and recovery bootloader install |
| [tools/](tools/) | BLE update, serial console, Wi-Fi provisioning, USB telemetry gateway and UART recovery |
| [docs/](docs/) | Project timeline, setup and implementation guides, validation and submissions |
| [keys/](keys/) and [patches/](patches/) | Team public verification key and dependency patches |

See the [project log](docs/PROJECT_LOG.md) for the course milestones and submission links.
