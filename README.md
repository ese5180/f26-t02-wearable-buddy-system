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

- **Roll call and map.** The browser connects to `BG-C01` over BLE and completes `SYNC`. Clicking **Roll call** writes `ROLL_CALL` (`op=3`). C01 asks each tag it has seen since boot for one recent valid fix over LoRa. C01 forwards each accepted fix as a `TAG_POSITION` event over BLE. T01 returned a valid GPS fix in the two-board test, and the team confirmed the Web map updates. A tag with no fresh fix leaves its previous map position and timestamp in place. [Web integration](docs/WEB_ROLL_CALL.md)
- **Presence, SOS and gather.** T01 broadcasts a BLE heartbeat. C01 reports online, missing and recovered states. The two-board tests covered the T01 Wio USER button sending an SOS and a C01 GATHER producing one short vibration with a LoRa ACK.
- **GPS and fleet management.** Both boards obtained GPS fixes outdoors. C01's Wi-Fi telemetry upload to Memfault was verified independently of the Web BLE session.

To reproduce the demo, build both roles using the [MVP handoff](docs/MVP_HARDWARE.md). Open the team's Web client in live BLE mode, connect to `BG-C01`, wait for T01 to appear online, and tap **Roll call** when the tag has a fresh fix. Look for a new **last-location time** even if the marker stays in the same place. The C01 USB command `bg locate` exercises the same LoRa path.

目前的实机组合是 T01 和 C01。网页点名后，C01 逐个向已登记的标签请求一次有效定位，标签通过 LoRa 回传，C01 再通过 BLE 更新网页。复现时先确认 T01 在线，再看地图的最后定位时间是否刷新。

### Next steps / 后续工作

1. **Complete the three-tag test.** Provision T02 and T03 with unique IDs and the same tag build profile. Check that one roll call produces one update per tag and that a missing or unresponsive tag does not stop the rest of the group. / 配好 T02、T03，验证一次点名、每标签一次更新，以及单个标签失联时其余标签照常回包。
2. **Show per-tag results in the Web UI.** Add explicit pending, no-fix and no-reply states. Keep the previous position visible with its age. Test reconnect and re-sync during a round. / 网页补齐等待定位、无有效定位和无回包状态，保留上次位置和时间，并测试断线重连。
3. **Validate the assembled wearable.** Repeat GPS cold starts and outdoor walks with the enclosure, antenna and battery installed. Test whether vibration or normal play triggers false IMU impact alerts. / 装壳和装电池后复测冷启动、户外收包与定位，并检查振动和正常活动造成的 IMU 误报。
4. **Rerun firmware-update and privacy checks.** Verify C01's signed BLE update workflow with the new GATT service and pairing settings, including disconnecting the browser for the maintenance session. Authenticate and encrypt the LoRa location frames and define device enrollment and location retention before a camp pilot. / 回归验证新版 C01 的 BLE 签名更新；试点前补 LoRa 位置消息认证和加密，明确设备登记及位置保留规则。

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
