# BuddyGuard — ESE 5180 Team 2

**BLE presence + LoRa P2P + signed BLE firmware updates**, on the XIAO ESP32S3 / Wio-SX1262 B2B kit.

**营地标签与辅导员监测原型**：BLE 通信心跳、LoRa P2P 查询／告警、MCUboot 签名 BLE 更新。

## Start here / 文档入口

| Document | 中文 | English |
|---|---|---|
| Teammate setup, build, provisioning, wireless demo, OTA and troubleshooting / 队友完整复现 | [操作文档](docs/zh/GETTING_STARTED.md) | [Reproduction guide](docs/en/GETTING_STARTED.md) |
| Module interfaces, threads, radio formats, update chain and limits / 固件解释 | [固件解释](docs/zh/FIRMWARE.md) | [Firmware explanation](docs/en/FIRMWARE.md) |
| Observed build and hardware results / 实测结果 | [Validation record](evidence/validation.md) | [Current build metadata](evidence/current-builds.json) |

Current source: **1.1.16**, modularized from the validated 1.1.15 baseline. Check the validation record for the final hardware outcome of each test. The branch is `codex/buddyguard-wireless-fota`; clone that branch while changes are under review.

当前源码 **1.1.16**，基于已验证 1.1.15 拆分模块。每项测试的最终实板结果见验证记录。审核期间请克隆 `codex/buddyguard-wireless-fota` 分支。

## Repository layout / 目录

```text
firmware/
  src/main.c          startup and service loop / 启动与主循环
  src/ble/            presence and maintenance / 广播与维护窗口
  src/roster/         counselor presence state / 辅导员名单
  src/radio/          LoRa event worker / 无线消息与重试
  src/drivers/        Wio-SX1262 hardware / 硬件配置与收发
  src/protocol/       portable wire codec / 纯 C 协议
  src/platform/       watchdog and image health / 看门狗与镜像确认
  src/console/        USB shell / 串口命令
  include/            service interfaces / 模块接口
scripts/              role-aware builds, backup, initial USB provisioning
                      分角色构建、整片备份、首次烧录
tools/                BLE SMP and serial host clients / 电脑端工具
tests/                host decoder + native firmware protocol tests
                      电脑解码与固件协议测试
docs/en/ docs/zh/      complete bilingual guides / 完整中英文文档
evidence/             curated validation and hashes / 验证摘要与哈希
keys/                 public verification key only / 仅公钥
west.yml              pinned Zephyr + imported modules / 固定依赖
```

Use workspace/source/build/key paths **without spaces**. The pinned board is `xiao_esp32s3/esp32s3/procpu`; Zephyr revision `f80761e49401497b5ee64ee9732dc2791055ec52`, SDK 1.0.1, west 1.5.0, Python 3.12. A new teammate should follow the complete guide rather than reuse another machine's absolute paths. `scripts/build.py --help` lists role, tag ID, workspace, key and isolated-version options.

源码／工作区／构建／密钥路径**不能带空格**。新队友请按完整操作文档建立自己的环境；脚本支持独立工作区与 T01–T03 配置，无需沿用开发 Mac 的绝对路径。

## Scope / 当前范围

The bench uses one tag and one counselor. Implemented: BLE presence/roster, LoRa STATUS/SOS/ACK/QUERY/REPLY, serial controls and signed BLE test updates. Planned product features — child-to-child pairing, IMU/clasp/motor/GNSS/battery readings and Wi-Fi/cloud — are **not implemented** here. Motion and clasp report UNKNOWN; battery reports unavailable. LoRa QUERY has no automatic retry and frames arriving during ACK waits can be lost. No location/distance estimate is produced.

目前实测为一标签＋一辅导员。已实现 BLE 名单、LoRa 消息、串口控制与签名更新；儿童配对、外设测量及 Wi-Fi／云尚未接入。QUERY 暂无自动重试，等待 ACK 可能丢失其他帧，不输出位置／距离。

MCUboot authenticates signed application images. Hardware secure-boot eFuses/flash encryption are not enabled; BLE SMP is unpaired, radio business frames are unauthenticated, and identifiers are static. Shared signing keys do not prevent selecting the wrong role image. This is a laboratory prototype, not validated child-safety equipment.

MCUboot 验证应用签名；尚未开启硬件 secure boot／Flash 加密，管理接入与业务帧认证尚待完成，ID 仍为静态。共用签名根不能防止刷错角色。本项目目前为实验原型。

## Files kept local / 留在本机的文件

Do not commit private keys, full-flash backups, credentials, `.venv`, build directories, signed artifacts or raw captures. The public verification key and curated non-secret evidence are included. Existing boards require the original team signing identity; the guides explain new-board provisioning separately.

私钥、整片备份、凭据、环境、构建、签名镜像及原始抓取不上传。已有板子必须使用原团队签名身份；新板首次安装有单独流程。

## Project references / 项目材料

- [Current BOM / 硬件清单](https://docs.google.com/spreadsheets/d/1GG7O95_0aSs89AQl4g6phD0VFDLTcqFAaMB5zflHOEA/edit?gid=2071228825#gid=2071228825)
- [Wireless demo assignment / 课程题目](https://docs.google.com/document/d/1gKqiMN2WEVwAf0uwNyS8Gh1MBPQqg_3FwDCZpiu_ctY/edit)
- [Hardware review / 硬件复核](docs/REVIEW-2026-10-30.md)
- [Pre-refactor README / 重构前记录](docs/archive/README-before-modularization.md)
- [Earlier concept / 旧概念归档](docs/archive/README-before-buddyguard.md)

Root `README.pdf` and `image/` are original course-history artifacts, not generated copies of the current firmware documentation. `patches/` is a historical software-SPI diagnostic; the current hardware-SPI build needs no patch.

根目录 `README.pdf`、`image/` 为早期课程材料，不是当前文档导出；`patches/` 仅供历史诊断参考。

Team Jet2Holiday: Siwei Lei, Rico Zhuang, Yunzhe Deng, Yubin Guan.
