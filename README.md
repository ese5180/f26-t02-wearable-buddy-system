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

The work below is pending. Prepare T02 and T03 in parallel with the enclosed IMU tests. Fix alert delivery and check-in timing before enabling the C01 motor. / 以下为待做项。摔倒台架测试和 T02、T03 的准备可并行，启用 C01 马达前先补齐告警送达和丢签到判断。

#### 1. Alert delivery and check-in / 告警送达与签到

- **SOS identity and priority.** Tags can reuse a LoRa sequence after reboot, and C01's current duplicate check uses tag, type and sequence. Add a tag boot/session ID and keep the same event ID across retransmissions. C01 should ACK retries and record each new alert once. Prioritize manual SOS and possible-fall messages ahead of routine status and queued position requests. / 给告警加入标签开机标识和事件序号，重发沿用同一个编号。C01 对重发继续回 ACK，只登记一次。手动 SOS 和疑似摔倒优先于普通状态和排队中的定位请求。[Radio dispatch](firmware/src/radio/link.c) · [Deduplication](firmware/src/radio/history.c)
- **Check-in timing.** Use a timestamp read after the heartbeat snapshot or a wrap-safe time comparison so a heartbeat received during polling cannot appear overdue. Report the first missing transition once. A new BLE heartbeat marks the tag recovered. A LoRa reply reports separate reachability and can supply a position. / 修正心跳时间差，防止刚收到的心跳被判成超时。首次丢签到只报一次，新 BLE 心跳标记恢复。LoRa 回包单独显示可达，并可补位置。[Roster](firmware/src/roster/roster.c)
- **Maintenance state.** The current build uses an 8 s ordinary timeout and a 600 s grace period based on the last maintenance heartbeat. Show maintenance separately on the Web and test the expiry and return to normal check-ins. / 当前普通超时 8 秒，最后收到维护心跳时宽限 600 秒。网页明确显示维护，并测宽限到期和恢复普通签到。[Timeout configuration](firmware/Kconfig)

#### 2. Fall, local vibration and location / 摔倒、振动与定位

- **Possible fall.** Secure the IMU in its enclosure. Evaluate an impact followed by a posture change or a short period of low motion. Give the event its own cause so C01 and the Web can distinguish it from manual SOS. Compare controlled fall simulations with walking, running, putting the device down and motor vibration. Record misses and false alerts. / 固定 IMU 后用撞击、姿态变化和随后动静判断疑似摔倒，单独上报原因。用受控模拟和日常动作测漏报、误报。[IMU](firmware/src/sensors/imu.c) · [Motion logic](firmware/src/sensors/motion.c)
- **C01 haptics.** Enable the DRV2605L motor on C01 and route new SOS, possible-fall and missing events to a local vibration queue. Keep alerts moving while the motor is busy and give urgent alerts priority. Repeated packets and the same ongoing missing state share one pulse. / C01 启用 DRV2605L，三个告警走本地振动队列。马达忙时保留新告警，紧急告警优先。重发和持续失联只振一次。[Haptics driver](firmware/src/actuators/haptics.c)
- **Send the alert first, then update its location.** Connect GPS reporting to the three alert paths above and keep the existing one-shot Web roll call. Associate each location result with the original tag and alert or roll-call round. Carry the sampling time or fix age through retries. / 手动 SOS、疑似摔倒和丢签到先发告警，再补位置。保留现有的一次点名一次回传。定位结果带对应的标签、告警或点名编号，以及采样时间或定位年龄。[Web roll call](docs/WEB_ROLL_CALL.md)

#### 3. Web alert handling / 网页告警处理

- Show tag, event type and event time. Track **Received**, **Seen by counselor**, and **Resolved** separately. A recovered heartbeat clears the missing state. Manual SOS and possible-fall alerts stay open until a counselor resolves them. / 显示标签、事件类型和时间。区分 C01 已收到、辅导员已查看和已处理。心跳恢复可清除失联状态，SOS 和疑似摔倒由辅导员处理后关闭。
- Show **Waiting**, **Fresh fix**, **No fix** (the tag answered without a valid position), or **No reply** (the LoRa request timed out). Keep the last known position and its age visible. / 定位明确显示等待、有效定位、已回包但无定位、超时未回包。保留上次有效位置和时间。
- Keep C01 local alerts working during a Web disconnect. On reconnect, sync open alerts and the latest tag positions without duplicating alerts. The current history is in RAM. Define how unresolved alerts are restored after a C01 restart, and expose any history gap to the counselor. / 网页断线时 C01 仍提醒，重连补回未处理告警和最新有效位置，避免重复弹报。当前记录在 RAM，需要补 C01 重启后未处理告警的恢复和历史缺失提示。[BLE sync](firmware/src/ble/app_ble.c)

#### 4. One C01 + three tags / 三标签验收

Provision T01, T02 and T03 with distinct IDs. Run the following cases against the same C01 and record the result and alert delay. / 使用三个不同标签 ID，在同一台 C01 上逐项实测并记录结果和告警延迟。

| Scenario / 场景 | Pass condition / 通过条件 |
| --- | --- |
| Normal check-ins and roll call / 正常签到并点名 | All three tags appear online. One click returns one position per tag with a fresh fix. / 三枚都在线，有有效定位的标签各更新一次。 |
| One tag loses BLE, then returns / 单标签丢签到再恢复 | C01 alerts once and requests that tag's position. A new BLE heartbeat clears its missing state. The other two tags remain active. / C01 提醒一次并查对应位置，心跳回来后恢复，其余标签照常。 |
| One tag has no fix, or one gives no reply / 一枚无定位或无回包 | The Web distinguishes the two outcomes and preserves the old location time. The other tags still update. / 网页区分两种结果，保留旧定位时间，其余标签继续更新。 |
| SOS during a roll call, plus a second tag alert while C01 vibrates / 点名途中 SOS，振动时另一枚报警 | SOS is shown promptly. Both new alerts are recorded and handled without duplicate pulses or a lost alert. / SOS 及时显示，两条新告警都记录和提醒，重发不重复振。 |
| Retry and tag reboot / 重发与标签重启 | A retry creates one alert in total. An SOS immediately after a tag reboot creates a new alert. / 同一事件重发只告警一次，标签重启后发出的新 SOS 单独告警。 |
| Web disconnect and C01 restart / 网页断开与 C01 重启 | C01 alerts locally while offline. On reconnect, the Web shows the unresolved alerts and positions, or explicitly reports a history gap after restart. / 断线仍有本地提醒，重连恢复未处理告警和位置。重启后若历史缺失要明确显示。 |
| Maintenance or brief packet loss / 维护或短时丢包 | Maintenance is visible, expiry restores normal timeout behavior, and valid recent heartbeats avoid a false missing alert. / 维护状态可见，到期恢复普通超时，近期有效心跳不会误报失联。 |

#### 5. Before a camp pilot / 试点前检查

- Repeat GPS cold starts and outdoor walks with the enclosure, antenna and battery installed. Verify C01's signed BLE update workflow with the Web service and pairing settings. / 装壳和电池后复测 GPS 冷启动、户外定位及新版 C01 的 BLE 签名更新。
- Assign each C01 an explicit tag roster. Authenticate and encrypt LoRa location messages, and define who can view locations and how long they are retained. / 每台 C01 按明确的组名单接收标签，给 LoRa 位置消息加认证和加密，明确查看权限及位置保留时间。

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
