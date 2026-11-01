# 下一阶段：云端设备管理

[English](../en/NEXT_STEPS.md) · [本轮无线／FOTA流程](WIRELESS_RELIABILITY.md)

团队确认目前还没有课程 Memfault／nRF Cloud 项目。云端集成尚未实现，本轮的 BLE FOTA、LoRa 与 UART 恢复不依赖云账号。

课程3.11要求版本／OTA、core dump 和至少一项关键设备数据上传。建议下一阶段先接辅导员 C01，儿童标签继续使用 BLE／LoRa。先与教学团队确认课程组织／项目的开通方式及 ESP32＋Zephyr 接入口径，再使用项目凭据；密钥不要发到聊天、README 或 Git。

按以下独立模块推进，避免把网络代码塞进 main 或 LoRa 驱动：

| 计划模块 | 职责 | 验收结果 |
|---|---|---|
| `network/wifi` | C01 联网、断线重连、连接状态 | 断网时本地监督继续工作，恢复网络后重连 |
| `telemetry/device_info` | 硬件标识、角色、应用版本 | 云端正确显示设备与版本 |
| `telemetry/metrics` | 上报 LoRa 未确认计数、失联事件、收到的运动状态等 | 云端出现至少一项真实关键数据，未知值保持 UNKNOWN |
| `platform/crash` | 崩溃记录和上传、匹配 ELF 符号 | 一次受控故障后能查看解析的 core dump |
| `update/cloud_fota` | 云端候选查询／下载，复用签名与 secondary | 云端触发签名更新，设备确认新版本，错误镜像不执行 |

这是后续设计草案，不代表这些源码已存在。先固定官方 SDK 版本并完成最小构建，再确定存储分区与接口。core dump 不能覆盖 MCUboot、primary、secondary；保留原团队签名根和未确认试运行机制。新加入的网络／TLS／崩溃存储功能需要重新检查 RAM、Flash、线程栈与电量预算。

[Memfault Zephyr 官方集成](https://docs.memfault.com/docs/mcu/zephyr-guide)提供纯 Zephyr 的 MCUboot FOTA 后端，可作为接入参考；[官方2026年5月更新](https://docs.memfault.com/changelog/2026/05/31)记录了 ESP32-S3 Zephyr 线程解析支持。仍需在本项目固定的 Zephyr 版本、分区和 XIAO 硬件上实际构建／测试，不能把文档支持等同于本机已经接入。

其他产品工作：儿童之间 buddy pairing、三标签并发、搭扣／马达／电池／GNSS，以及管理授权、业务帧认证和防重放。普通 BLE 失联检测不输出儿童位置或距离，签名固件也不等于无线数据已经保密。

课程后续交付还包括：每位队友独立拉取／构建／烧录验证，Zephyr Ztest／HIL 与 PPK2 功耗测量，以及 Git hooks／可复现 CI。当前电脑端测试不能描述为已完成 Zephyr Ztest 或 CI。
