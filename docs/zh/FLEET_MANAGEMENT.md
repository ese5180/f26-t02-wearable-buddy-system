# Fleet Management / 设备云端管理

[English](../en/FLEET_MANAGEMENT.md) · [验收操作卡](FLEET_CHECKOFF.md) · [A/B 实验](FLEET_AB_TEST.md) · [实测记录](../VALIDATION.md)

`--wifi` 让 XIAO 直接通过 2.4 GHz PSK Wi-Fi 连接 Memfault，上传系统与产品指标、重启原因和 coredump，并下载签名固件。普通 BLE/LoRa 构建仍可独立使用。`--fleet` 也可以单独启用，供台架 USB 转发。

## 1. 构建

按[环境指南](GETTING_STARTED.md)安装固定版本的 Zephyr、SDK 和两个 Python 环境。新版 `west.yml` 固定 Memfault SDK 1.46.0。复用 manifest 仍指向 Zephyr 的老工作区时，若 SDK 尚未安装，在 `$BG_WS/modules/lib/` 下补齐：

```sh
git clone --depth 1 --branch 1.46.0 https://github.com/memfault/memfault-firmware-sdk.git \
  "$BG_WS/modules/lib/memfault-firmware-sdk"
```

从本仓库根目录构建。设置 `BG_WS` 为实际工作区，使用原团队私钥。以下为 A/B 基线示例，新发布请另选未使用的版本号和构建目录。

```sh
BG_WS="$HOME/buddyguard-workspace"
BG_KEY="$HOME/.config/buddyguard/team2-signing/firmware-ecdsa-p256.pem"
"$BG_WS/.venv/bin/python" scripts/build.py --workspace "$BG_WS" --key "$BG_KEY" \
  --build-dir "$BG_WS/build-bg-wifi-t01" --role tag --tag-id 1 --imu --wifi --version 1.5.0
"$BG_WS/.venv/bin/python" scripts/build.py --workspace "$BG_WS" --key "$BG_KEY" \
  --build-dir "$BG_WS/build-bg-wifi-c01" --role counselor --wifi --heartbeat-seconds 15 --version 1.5.1
```

未接 LSM6DSO 时省略 `--imu`。`--wifi` 构建会应用仓库内 Zephyr 共享堆加锁补丁，构建报告记录补丁哈希、签名镜像和 ELF 路径。用 `firmware/zephyr/zephyr.signed.bin` 更新，用同次构建的 `zephyr.elf` 在 Memfault **Software → Symbol Files** 上传符号。每个角色和每次重建都要用匹配的 ELF。

| 角色 | 设备 ID | Software type | Hardware version |
|---|---|---|---|
| 标签 | BG-T01 / BG-T02 / BG-T03 | buddyguard-tag | xiao-esp32s3-wio-tag-v1 |
| 辅导员 | BG-C01 | buddyguard-counselor | xiao-esp32s3-wio-counselor-v1 |

## 2. 安装与配置

已有团队 MCUboot 的设备先按[环境指南](GETTING_STARTED.md)的 BLE 更新步骤安装各自的 Wi-Fi 基础镜像，检查 `bg status` 的版本、角色及 `confirmed=1`。通过团队渠道取得 [BuddyGuard 云端项目](https://app.memfault.com/organizations/university-of-pennsylvania-w1vt/projects/nrf-project/devices)的访问权限和 Project Key。

创建权限为 600 的 `$HOME/.config/buddyguard/wifi.json` 和 `$HOME/.config/buddyguard/memfault-project-key`，后者只放项目密钥。Wi-Fi 文件格式如下，密码在本机填写：

```json
{"ssid": "AirPennNet-Device", "security": "psk", "password": "REPLACE_LOCALLY"}
```

用 `.venv/bin/python -m serial.tools.list_ports -v` 枚举设备，并设好 `TAG_PORT`、`COUNSELOR_PORT`。校园 Device Portal 需注册 **Wi-Fi STA MAC**，可用以下命令加 `--status` 查询。正式配置时去掉 `--status`：

```sh
.venv/bin/python tools/bg_wifi.py --port "$TAG_PORT" --device BG-T01
.venv/bin/python tools/bg_wifi.py --port "$COUNSELOR_PORT" --device BG-C01
```

工具核对设备 ID 和确认状态，将配置存入 NVS 后重启。设置会跨 OTA 保留，台架 NVS 未加密。Wi-Fi worker 等待 DHCP，校时后用 HTTPS 上传，并约每 30 秒检查适用发布。`bg_wifi status` 查看配置，`bg_wifi check` 请求立即检查。打开云端 **Devices → Reports** 核对设备和版本。

## 3. 发布、诊断和验收

1. 在 Memfault 创建 Release，按表中角色上传匹配版本的签名应用与符号文件，向目标 cohort 激活。使用未预先确认的应用镜像。设备收全镜像并 flush 尾块后请求 MCUboot 试运行。重启后用 `bg status` 核对版本和 `confirmed=1`，再核对云端版本。
2. 在 **Reports** 查看 Zephyr 的 CPU、堆和线程指标。供电相关字段 `bg_usb_host_active` 观测 USB 主机活动，无法识别实际电源来源或电池 SoC。真实业务字段包括 C01 的 `bg_missing_events_total` 和 T01 的 IMU 状态。
3. 用 `bg reboot` 生成 User Reset。在已确认的台架镜像上用 `mflt test assert` 生成 coredump，重启后让 Wi-Fi 上传，在 **Reboots / Traces** 核对原因和符号化调用栈。完整演示命令见[验收操作卡](FLEET_CHECKOFF.md)。

Wi-Fi OTA 与 BLE SMP 共用 secondary，演示时一次只操作一条更新路径。Memfault coredump 使用 secondary 尾部的预留空间，先上传崩溃报告再做 BLE 更新或擦除 secondary。Wi-Fi 下载失败后重试整包。A/B 策略调整的是诊断采集周期，云端上传周期独立。

## 4. 可选 USB 诊断路径

单独用 `--fleet` 构建、安装。释放该端口的串口监视器，再启动网关：

```sh
.venv/bin/python tools/bg_fleet.py --port "$TAG_PORT" --device BG-T01 \
  --key-file "$HOME/.config/buddyguard/memfault-project-key" --seconds 180
```

网关从串口读取 Memfault chunks，持久化到本机 SQLite 后上传，失败保留重试。此配置用 BLE SMP 安装更新。直连 Wi-Fi 演示使用设备自己的上传链路。

参考：[Memfault Zephyr 接入](https://docs.nrfcloud.com/docs/mcu/zephyr-guide) · [Penn 设备 Wi-Fi](https://isc.upenn.edu/using-wireless-penn)。
