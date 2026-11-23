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
- **On-demand GPS.** The browser requests a roll call over BLE. C01 then asks each tag it has seen since boot for one recent valid fix over LoRa and forwards each accepted fix as a BLE `TAG_POSITION` event. The two-board test returned a valid T01 fix, and the team confirmed the Web map updates. A tag with no fresh fix leaves its previous map position and timestamp in place. / GPS 已实现按需回传。点一次名，标签通过 LoRa 回一次有效定位，由 C01 转给网页。[Web integration](docs/WEB_ROLL_CALL.md)
- **IMU, SOS and gather.** T01 reports moving or still. A strong impact currently triggers the same SOS path as the Wio USER button. The bench tests verified button SOS and a GATHER request that started one short vibration with a LoRa ACK. / 当前 IMU 能判断动静和撞击，撞击会触发 SOS。摔倒识别是下一阶段。
- **GPS and fleet management.** Both boards obtained GPS fixes outdoors. C01's Wi-Fi telemetry upload to Memfault was verified independently of the Web BLE session.

To reproduce the demo, build both roles using the [MVP handoff](docs/MVP_HARDWARE.md). Open the team's Web client in live BLE mode, connect to `BG-C01`, wait for T01 to appear online, and tap **Roll call** when the tag has a fresh fix. Look for a new **last-location time** even if the marker stays in the same place. The C01 USB command `bg locate` exercises the same LoRa path.

复现时先确认 T01 在线、有有效定位，再点一次名，检查网页最后定位时间是否刷新。

### Next steps / 后续工作

1. **Build and test IMU fall detection.** With the IMU fixed in the enclosure, evaluate whether an impact followed by a posture change or a short period of low motion can separate a possible fall from an ordinary knock. Use controlled simulations and ordinary walking, running, putting the tag down and motor vibration as comparison cases. Verify that one simulated fall raises one **Possible fall** alert on the Web UI and that the manual SOS remains usable. / IMU 固定在外壳后，用撞击、姿态变化和随后动静做判断。用模拟跌倒和日常动作测漏报、误报。验收时网页收到一次「疑似摔倒」，实体 SOS 照常可用。
2. **Run the one-counselor, three-tag test.** Provision T02 and T03 with distinct IDs. Verify that C01 shows all three routine check-ins, detects and recovers one missing tag, and receives one location per tag with a fresh fix after one roll call. Repeat with one tag offline and one without a GPS fix. Check that the other tags still update. / 配好 T02、T03，验收三标签签到、单标签失联与恢复、一次点名多标签定位，以及某个标签掉线或无定位时其余标签仍能更新。
3. **Show per-tag roll-call results in the Web UI.** Add explicit pending, no-fix and no-reply states. Keep the previous position visible with its age. Test reconnect and re-sync during a round. / 网页补齐等待、无定位和无回包状态，保留上次位置和时间，并测试断线重连。
4. **Validate the assembled wearable and signed updates.** Repeat GPS cold starts and outdoor walks with the enclosure, antenna and battery installed. Verify C01's signed BLE update workflow with the new GATT service and pairing settings. / 装壳和装电池后复测 GPS 冷启动与户外定位，并回归新版 C01 的 BLE 签名更新。
5. **Prepare location handling for a pilot.** Keep GPS transmission on demand for the MVP. An SOS can later request a fresh fix or show a timestamped last-known position. Authenticate and encrypt location messages over LoRa and define enrollment and retention before a camp pilot. / MVP 保持按需传位置。后续可给 SOS 加上请求定位或显示带时间的最近位置。营地试点前补齐 LoRa 位置消息认证、加密、设备登记和位置保留规则。

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
