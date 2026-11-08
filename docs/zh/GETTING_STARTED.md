# BuddyGuard：队友复现操作文档

[Fleet Management 构建与 Wi-Fi 配置](FLEET_MANAGEMENT.md)

[可选 SparkFun LSM6DSO 接入文档](IMU_LSM6DSO.md)

[English](../en/GETTING_STARTED.md) · [固件解释](FIRMWARE.md) · [验证记录](../VALIDATION.md)

适用硬件：**XIAO ESP32S3＋Wio-SX1262 B2B 套装**。本流程复现当前 Zephyr＋MCUboot 的 BLE、LoRa 和签名更新演示。先用 **1 个标签 T01＋1 个辅导员端 C01**；代码支持 T01–T03，但尚未完成四设备整套验证。当前不需要外接 IMU、GPS、电池、马达或屏幕，USB 负责供电。

## 1. 装配硬件

先断开 USB，把 Wio 与 XIAO **上下叠合，扣紧底部 B2B 插座**。两块板只放在旁边并不会连接 SPI。Wio 接 915 MHz 天线，XIAO 接 2.4 GHz 天线，然后分别插入 USB **数据**端口。Hub 的 PD 输入可能只供电、不识别串口；LED 亮不代表电脑已识别。天线尽量避开金属、在桌面分开摆放。

B2B 套装无需杜邦线，不要套用 nRF52840 通孔模块的针脚表。[官方装配说明](https://wiki.seeedstudio.com/wio_sx1262_with_xiao_esp32s3_kit/)用于核对叠合方式，GPIO 表见[固件解释](FIRMWARE.md#硬件与射频配置)。

## 2. 从零建立开发环境

已验证主机为 Apple Silicon Mac，Python 3.12、west 1.5.0、Zephyr SDK 1.0.1。其他系统需要另外配置 USB/BLE，团队尚未验证。以下假设已安装 Homebrew。**源码、工作区、构建和密钥路径都不要带空格。**

```sh
xcode-select --install  # 只需一次；已安装命令行工具可跳过
brew install cmake ninja gperf ccache dtc wget python@3.12
BG_WS="$HOME/buddyguard-workspace"
mkdir -p "$BG_WS"
cd "$BG_WS"
python3.12 -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install west==1.5.0
git clone --branch codex/buddyguard-wireless-fota https://github.com/ese5180/f26-t02-wearable-buddy-system.git buddyguard
west init -l buddyguard
west update
west zephyr-export
west packages pip --install
west blobs fetch hal_espressif
west sdk install --version 1.0.1 --gnu-toolchains xtensa-espressif_esp32s3_zephyr-elf
cd buddyguard
python3.12 -m venv .venv
.venv/bin/python -m pip install -r requirements-host.txt
```

两个 Python 环境用途不同：**工作区 `.venv`** 提供 west/esptool/imgtool，负责构建和首次烧录；**仓库 `.venv`** 提供 BLE、串口工具。后续请使用示例中的显式路径，不能假设激活一个环境就包含全部依赖。

`west.yml` 固定 Zephyr 为 `f80761e49401497b5ee64ee9732dc2791055ec52`，版本显示 4.4.99，是固定开发版本而非正式 release。`west update` 同时解析其模块版本。ESP32 蓝牙配置会检查整个 hal_espressif 的 blob 集合，因此不要只下载 ESP32S3 的部分 blob。**正常 BLE／LoRa 固件不需要修改 Zephyr 源码**；可选 UART recovery 会自动应用一条 MCUboot Kconfig 默认值修复，见 [UART 恢复文档](UART_RECOVERY.md)。

复用已有工作区时，把 `BG_WS` 改成其路径并核对版本，不要重复 `west init`。SDK 安装需要网络和磁盘空间。主机依赖参考 [Zephyr 安装指南](https://docs.zephyrproject.org/latest/develop/getting_started/index.html)，板卡参考 [XIAO ESP32S3 文档](https://docs.zephyrproject.org/latest/boards/seeed/xiao_esp32s3/doc/index.html)；遇到新版示例差异时，以本仓库固定的版本为准。

## 3. 准备签名身份

**已装 Team 2 引导程序的现有板子：**通过团队私下受控渠道取得原有开发签名私钥，或者直接由签名电脑提供固件。Git 中只有 `keys/team2-signing-public.pem`。私钥放在仓库之外，例如：

```sh
BG_KEY="$HOME/.config/buddyguard/team2-signing/firmware-ecdsa-p256.pem"
chmod 700 "$(dirname "$BG_KEY")"
chmod 600 "$BG_KEY"
```

上述命令假设私钥已私下放好，不会替你生成文件。不要把私钥贴进聊天、上传 Git 或放到源码中。每人另建一把私钥，不能给已有 Team 2 板子做 OTA。

**全新、尚未首次烧录的开发板**可以建立单独实验身份，但首次 MCUboot 和今后所有更新必须使用同一把密钥：

```sh
mkdir -p "$HOME/.config/buddyguard/my-lab"
chmod 700 "$HOME/.config/buddyguard/my-lab"
BG_KEY="$HOME/.config/buddyguard/my-lab/firmware-ecdsa-p256.pem"
# 仅在此路径还没有密钥时运行；之后一直保存该密钥，不要覆盖。
"$BG_WS/.venv/bin/imgtool" keygen -k "$BG_KEY" -t ecdsa-p256
chmod 600 "$BG_KEY"
```

这不是更新已有 Team 2 板子的办法，而是为新板建立另一套信任根。

## 4. 分角色构建

在仓库根目录，确认 `BG_WS`、`BG_KEY` 已设置：

```sh
"$BG_WS/.venv/bin/python" scripts/build.py --workspace "$BG_WS" --key "$BG_KEY" --build-dir "$BG_WS/build-bg-tag1" --role tag --tag-id 1
"$BG_WS/.venv/bin/python" scripts/build.py --workspace "$BG_WS" --key "$BG_KEY" --build-dir "$BG_WS/build-bg-counselor" --role counselor
```

增加 T02/T03 时使用 `--tag-id 2` / `--tag-id 3`，并换独立构建目录。设备名自动生成 BG-T01/BG-T02/BG-T03、BG-C01；辅导员 ID 为 200。旧参数 `--counselor` 仍可用。

已接 LSM6DSO 时，按可选 IMU 指南，在标签首次构建及后续候选构建中都加入 `--imu`。仅做双板无线演示时省略；故障注入只用于后面的可选回滚演示，不用于正常版本。

脚本每次明确设置角色、扫描模式、ID、故障开关；同一构建目录换角色、ID、密钥或故障配置会被拒绝。遇到拒绝时用新目录，不要手动混改生成文件。

| 构建目录内文件 | 用途 |
|---|---|
| `firmware/zephyr/zephyr.signed.bin` | BLE OTA 使用的已签名应用 |
| `mcuboot/zephyr/zephyr.bin` | 仅首次 USB 安装引导程序使用 |
| `buddyguard-build.json` | 角色、版本、Zephyr revision、大小及哈希 |
| `signing-public.pem` | 从所选私钥导出的公钥，不含私钥 |

构建结束会验证镜像签名。**目前共用签名根不校验目标角色**，因此标签／辅导员镜像必须自己选对；文件名并不是引导程序的角色限制。

## 5. 确认端口与权限

```sh
.venv/bin/python -m serial.tools.list_ports -v
```

根据实际输出设置变量，下面只是示例；重插后端口可能变化：

```sh
TAG_PORT=/dev/cu.usbmodem101
COUNSELOR_PORT=/dev/cu.usbmodem1101
```

Mac 弹窗时允许终端／宿主进程访问蓝牙。**同一个串口只允许一个读取程序，同一块板只开一个 BLE 管理连接。**先关闭 Arduino 等其他串口监视器。

## 6. 首次 USB 烧录：仅新板需要

已有 Team 2 MCUboot 且能响应 `bg status` 的板子跳到第 8 节，用 BLE 更新。首次脚本会**擦除整片 Flash**，所以必须先给每块板建立各自的 8 MiB 原始备份。备份脚本只读 Flash，但会复位板子进出 USB loader。原始备份保存在 Git 外，不能覆盖。

```sh
"$BG_WS/.venv/bin/python" scripts/backup_flash.py --port "$TAG_PORT" --output-dir "$HOME/buddyguard-backups/tag1-original"
"$BG_WS/.venv/bin/python" scripts/flash_initial.py --build-dir "$BG_WS/build-bg-tag1" --port "$TAG_PORT" --backup-info "$HOME/buddyguard-backups/tag1-original/backup-info.json" --log-prefix "$HOME/buddyguard-logs/tag1-initial"
```

辅导员端重复上述流程，但要换**新的备份目录**、辅导员端口和 `build-bg-counselor`。脚本核验备份大小／哈希，存在设备身份时进行匹配，并检查生成的镜像写入地址。备份 `.bin` 与 `backup-info.json` 要一起保留。

烧录脚本会抓启动日志，应看到同步字 `0x1424`、开始广播和 `confirm result=0`。如 USB 自动进入下载模式失败，按[板卡文档](https://docs.zephyrproject.org/latest/boards/seeed/xiao_esp32s3/doc/index.html#programming-and-debugging)使用 BOOT/RESET，再识别端口重试，不要删除原始备份。不要烧写 eFuse。

## 7. 双板无线演示

先检查角色、版本和 `confirmed=1`，关闭两端维护接入窗口：

```sh
.venv/bin/python tools/bg_serial.py --port "$TAG_PORT" command bg status
.venv/bin/python tools/bg_serial.py --port "$COUNSELOR_PORT" command bg status
.venv/bin/python tools/bg_serial.py --port "$TAG_PORT" command bg maintenance 0
.venv/bin/python tools/bg_serial.py --port "$COUNSELOR_PORT" command bg maintenance 0
```

等待辅导员收到标签的新正常广播。打开**第二个终端**，重新设置辅导员端口变量，运行：

```sh
.venv/bin/python tools/bg_serial.py --port "$COUNSELOR_PORT" --log local/logs/my-wireless-demo.log monitor --seconds 90
```

第一个终端控制标签（其串口仍空闲）：

```sh
.venv/bin/python tools/bg_serial.py --port "$TAG_PORT" command bg sos
.venv/bin/python tools/bg_serial.py --port "$TAG_PORT" command bg heartbeat off
# 等待约 12 秒，再恢复：
.venv/bin/python tools/bg_serial.py --port "$TAG_PORT" command bg heartbeat on
```

辅导员应依次显示：周期 STATUS `RX ... type=1`；SOS `type=3` 和 `ACTION`；失联约 8–10 秒后 `BLE MISSING` → `QUERY` → `RX ... type=5`；恢复后 `RECOVERED`。LoRa 回复只说明 LoRa 可达，不能清除 BLE 失联状态。

QUERY 最多发送 3 次，并在等待时继续处理其他帧；全部超时则明确报告未确认，操作员可再发起查询。已有 monitor 时用 `command --no-read` 发命令，见[无线帧与重试说明](FIRMWARE.md#无线帧格式)。SOS 入队成功也不代表已送达，要看辅导员接收和标签 ACK。

这里的 heartbeat 是**通信心跳**，不是生理脉搏。`heartbeat off` 仅停 BLE 广播，LoRa 继续工作。演示不输出距离或位置；未启用 `--imu` 时 motion 为 UNKNOWN；可选 IMU 文档说明真实采样。clasp 仍 UNKNOWN，电量 unavailable。

## 8. BLE 签名固件更新

给候选镜像使用独立版本／构建目录，例如 `--version 1.1.21`，必须保持**目标板相同角色和 ID**。版本覆盖使用隔离源码副本，不修改仓库 VERSION。标签候选构建示例：

```sh
"$BG_WS/.venv/bin/python" scripts/build.py --workspace "$BG_WS" --key "$BG_KEY" --build-dir "$BG_WS/build-bg-tag1-candidate" --role tag --tag-id 1 --version 1.1.21
.venv/bin/python tools/bg_serial.py --port "$TAG_PORT" command bg maintenance 300
# 先等约 4 秒，让辅导员收到 maintenance 标志，再开始连接：
.venv/bin/python tools/bg_ble.py --name BG-T01 images
.venv/bin/python tools/bg_ble.py --name BG-T01 update "$BG_WS/build-bg-tag1-candidate/firmware/zephyr/zephyr.signed.bin"
```

默认 SMP 数据包上限 244 字节，约 435 KB 的应用需要几分钟，期间保持供电。工具在传输错误后可重连，按同一文件 SHA 续传。上传后请求 **test boot**，不会由电脑直接永久确认。可以另开目标串口监视器抓启动日志。

验收必须看到：目标新版本启动，至少 15 秒健康运行后 `Image health check complete; confirm result=0`；再次 `bg status` 和 `images` 确认**目标版本 active 且 confirmed**。窗口过期时重新 `bg maintenance 300` 再查。更新 C01 时用 `--name BG-C01`、辅导员端口及其镜像。

BLE SMP 连接期间标签暂停广播。如果辅导员最后看到维护标志，改用 **600 秒宽限**而非正常 8 秒；长时间／卡住的更新仍可能报警。默认 300 秒只限制新连接接入，已建立的上传可继续完成。

可选安全更新复现：

1. **中断续传：**`upload IMAGE --stop-at 20`，确认旧版本仍 confirmed，再用同一文件 `update IMAGE`，检查从非零偏移续传。
2. **故障回滚：**先确认已知正常镜像。独立目录构建相同角色／ID、`--fault --version 1.2.2`，BLE 更新后应看到故障标志、WDT 复位和恢复原 confirmed 版本。故障镜像不能当正式版本。
3. **错误密钥／损坏拒绝：**历史完整 BLE 测试见验证记录。重做时用单独外部测试私钥，或复制镜像后破坏其签名覆盖数据；复位后检查旧 confirmed 版本保留。不要替换团队信任根或已知正常镜像。

## 9. 排障与交接

| 现象 | 检查 |
|---|---|
| LED 亮但没有串口 | USB 数据线、Hub 数据口；只有供电不够 |
| 同步字是 `0xffff` 而非 `0x1424` | 断电重扣 B2B；核对 GPIO；保持上游 GPIO 定义 |
| 找不到 west/sdk 命令 | 在已初始化工作区，用工作区 `.venv/bin/west` |
| ESP blob 检查失败 | 固定工作区中 `west blobs fetch hal_espressif` |
| 构建角色／ID 冲突 | 每个角色、ID、故障和签名身份换独立目录 |
| 无法 BLE 连接 | `bg maintenance 300`、蓝牙权限、关闭其他 BLE 客户端 |
| 停广播 8 秒还不报警 | 先关维护窗口，并让 C01 收到新正常标签广播 |
| 上传成功却还是旧版本 | 查引导／串口／image state；上传成功不等于签名接受 |
| LoRa 偶尔未确认或 QUERY 无回复 | 手动重试；现有桌面重试不是安全保障 |

交接前恢复 `bg heartbeat on`，确认两端角色及 `confirmed=1`，记录 Git commit 和 `buddyguard-build.json`，保存不含秘密的验证摘要，关闭串口／BLE 工具。Git 只上传核心源码、脚本、文档、公钥；私钥、整片备份、构建、`.venv`、签名产物和原始抓取留本机。描述项目时请保留[固件限制](FIRMWARE.md#已实现范围与限制)，不要把原型说成已完成的儿童安全产品。
