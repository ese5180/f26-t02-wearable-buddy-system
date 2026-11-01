# MCUboot UART 恢复：Team 2 操作流程

[English](../en/UART_RECOVERY.md) · [安全作业回答](FIRMWARE_SECURITY_CHECKOFF.md)

这是 **MCUboot 自身提供的 UART 恢复**，与 BLE FOTA、ESP ROM/esptool 烧录及应用的 `bg` 命令分开。用一块辅导员端演示，保留完整备份、原团队签名私钥和可用的辅导员签名镜像。此恢复路径**直接覆盖 primary 应用槽**，上传中断或错误签名可能导致应用不能启动，但仍可通过 MCUboot UART 恢复。

## 1. 接线和端口

XIAO 用自己的 USB-C 供电，串口适配器必须为 **3.3V 逻辑电平**。

| USB–TTL 信号 | XIAO ESP32S3 |
|---|---|
| TX | D7 / RX / GPIO44 |
| RX | D6 / TX / GPIO43 |
| GND | GND |
| VCC、DTR、RTS | 不接 |

这次实测的无品牌 CP2102 线为绿 TX、白 RX、黑 GND，红线悬空。**其他线不能只凭颜色判断针脚和电平**；USB D+/D− 不是 UART TX/RX。本次 Mac 的 TTL 端口是 `/dev/cu.usbserial-0001`，XIAO 自带 USB 是 `/dev/cu.usbmodem11301`。重新插拔后先查端口：

```sh
.venv/bin/python -m serial.tools.list_ports -v
```

按复现文档建立工作区，在仓库根目录操作；源码、构建、工作区、密钥路径均不要含空格。下面变量要改成本机实际值：

```sh
BG_WS="$HOME/buddyguard-workspace"
BG_KEY="$HOME/.config/buddyguard/team2-signing/firmware-ecdsa-p256.pem"
BG_BUILD="$BG_WS/build-bg-c01-recovery"
BG_USB=/dev/cu.usbmodem11301
BG_TTL=/dev/cu.usbserial-0001
```

## 2. 构建恢复 bootloader 与签名应用

```sh
"$BG_WS/.venv/bin/python" scripts/build.py --workspace "$BG_WS" --key "$BG_KEY" \
  --build-dir "$BG_BUILD" --role counselor --serial-recovery --version 1.1.20
```

改变角色、密钥或 recovery profile 时用新的构建目录。`--version` 生成独立源码副本，不修改仓库 `firmware/VERSION`。脚本验证签名并把大小、哈希写入 `buddyguard-build.json`。

**固定依赖版本所需修复：**脚本会幂等应用 [MCUboot 线程配置补丁](../../patches/mcuboot-esp32-uart-threading.patch)，修改工作区 `bootloader/mcuboot` 的一条 Kconfig 默认值：ESP32 启用串口恢复时使用多线程。实测原先的 Xtensa 单线程 bootloader 在 UART FIFO 中收到数据，但没有处理串口适配层的接收中断；单写 `CONFIG_MULTITHREADING=y` 无法改变该隐藏配置。构建脚本会检查最终符号并阻止超过 64 KiB 的 bootloader。不启用 recovery 的普通构建保留上游默认值，不需要临时诊断源码。

最终 bootloader 为 **63,728 字节**，现有 64 KiB 分区还剩 **1,808 字节**。之前 60,624 字节的可行性版本缺少上述修复。配置保留 primary 签名验证，选择 UART0、名义三秒入口窗口，以及没有可启动应用时的持续恢复。USB 日志单独保留。查询镜像时 MCUboot 会同步计算槽位哈希，客户端因此使用 15 秒查询超时。

## 3. 完整备份，再只安装 bootloader

关闭所有占用 XIAO 自带 USB 的监视器。备份目录必须在仓库之外，而且不能已存在：

```sh
BG_BACKUP="$HOME/buddyguard-backups/c01-before-recovery"
BG_INSTALL="$HOME/buddyguard-backups/c01-recovery-install"
"$BG_WS/.venv/bin/python" scripts/backup_flash.py --port "$BG_USB" \
  --output-dir "$BG_BACKUP" --baud 115200
"$BG_WS/.venv/bin/python" scripts/install_recovery_bootloader.py \
  --build-dir "$BG_BUILD" --backup-info "$BG_BACKUP/backup-info.json" \
  --port "$BG_USB" --output-dir "$BG_INSTALL"
```

第二条命令只是预检查；看到 `preflight: passed` 后，再重复它并加 **`--write`**。脚本核对 8 MiB 备份哈希、设备 USB 身份、团队公钥、ESP 镜像头、分区大小及 recovery/签名配置，只往 `0x0` 写 bootloader，随后整片读回。必须同时看到 `bootloader_matches=true` 和 `remaining_flash_preserved=true`；`0x10000` 及之后的全部内容必须与备份一致。本次 Hub 在更高波特率下备份失败，因此默认用115200。

**这里不要用 `flash_initial.py`**，那个首次安装流程会整片擦除。普通 BLE 应用更新不能安装新的 MCUboot。

## 4. 进入 MCUboot UART 恢复

终端 A：

```sh
.venv/bin/python tools/bg_uart_recovery.py --port "$BG_TTL" --wait 60 images
```

工具等待时按一下板子 **RESET**。不要按住 BOOT/GPIO0，那会进入 ESP ROM 下载器。应用还正常时，也可以在终端 B 触发复位：

```sh
.venv/bin/python tools/bg_serial.py --port "$BG_USB" command bg reboot
```

成功后工具输出槽位版本、哈希以及 `recovery_connected`。MCUboot 收到有效 SMP 命令后会保持恢复状态，直到复位；没有命令则在入口窗口后启动原应用。处于恢复状态时，应用的 `bg` 命令不能使用。同一个串口只能有一个读取程序。

## 5. UART 上传有效签名应用并启动

MCUboot 已在恢复状态时运行：

```sh
.venv/bin/python tools/bg_uart_recovery.py --port "$BG_TTL" --wait 60 \
  upload "$BG_BUILD/firmware/zephyr/zephyr.signed.bin" --reset
```

脚本先用 `keys/team2-signing-public.pem` 离线验证签名，显示上传进度，查询镜像并要求复位。UART 直接写 primary，不发送 BLE 更新的 test/confirm 命令。随后查看应用：

```sh
.venv/bin/python tools/bg_serial.py --port "$BG_USB" monitor --seconds 35
.venv/bin/python tools/bg_serial.py --port "$BG_USB" command bg status
```

核对实际运行版本、`COUNSELOR`、LoRa `0x1424` 读回、BLE 启动和最终 `confirmed=1`。UART 恢复不等同于 BLE 的试运行／自动回退流程；不能从“上传成功”推断固件已被接受或健康检查通过。

## 6. 错误签名测试，再恢复有效镜像

另外生成一次测试私钥，放在 Git 外，不能覆盖原团队密钥：

```sh
BG_BAD_KEY="$HOME/.config/buddyguard/team2-signing/wrong-key-test.pem"
# 仅当这个测试路径尚不存在时运行 keygen。
"$BG_WS/.venv/bin/imgtool" keygen -k "$BG_BAD_KEY" -t ecdsa-p256
chmod 600 "$BG_BAD_KEY"
"$BG_WS/.venv/bin/python" scripts/build.py --workspace "$BG_WS" --key "$BG_BAD_KEY" \
  --build-dir "$BG_WS/build-bg-c01-wrong-key" --role counselor --version 1.3.1
```

**绝对不要安装此构建的 MCUboot。**只取它的签名应用作为错误根测试。构建时它会通过自己的测试公钥验证，而使用团队公钥验证必须失败。

先启动以下工具，再复位进入恢复：

```sh
.venv/bin/python tools/bg_uart_recovery.py --port "$BG_TTL" --wait 60 \
  upload "$BG_WS/build-bg-c01-wrong-key/firmware/zephyr/zephyr.signed.bin" \
  --allow-invalid-test --reset
```

`--allow-invalid-test` 明确允许这一次负面测试绕过电脑端的团队公钥拒绝，并覆盖 primary。记录 USB 启动结果：**1.3.1 不应执行**。等待超过正常应用启动时间后，再查询 recovery；如果没有有效应用，`BOOT_SERIAL_NO_APPLICATION` 会让恢复服务持续可用。镜像查询哈希和上传成功本身不代表签名验证成功。

然后按第5步重新上传正确镜像，复位并核对版本、外设和确认状态。没有应用时，这一步不依赖 BLE 正常工作。完成所有 BLE 检查后执行 `bg maintenance 0`；需要重复两板 LoRa／IMU 回归时再接回标签端。

## 当前限制

UART 直接覆盖 primary，不能继承 BLE secondary 暂存带来的旧应用保留保证。没有串口访问身份认证、防降级策略或 ESP 硬件 secure-boot eFuse／Flash 加密。这项演示验证的是 MCUboot 对应用签名的执行约束。私钥和完整 Flash 捕获不上传 Git，只提交代码、公钥和整理后的验证记录。

协议参考：[MCUboot serial recovery](https://docs.mcuboot.com/serial_recovery.html)。
