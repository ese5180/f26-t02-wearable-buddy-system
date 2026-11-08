# Fleet Management checkoff 操作卡

队友于 2026-11-08 准备了下方 1.5.x 演示。开始前核对云端 release 和两端实际版本，若已演示过则准备新的签名版本。按顺序执行：分组/OTA → A/B 自然采集对照 → 系统与供电指标 → 失联数据 → 图表 → 重启原因 → 崩溃现场。

## 0. 终端准备

在仓库根目录的同一个终端执行。关闭其他串口监视器和 `bg_fleet.py`，当前上传由板子通过 Wi-Fi 完成。

```sh
BG_PY=.venv/bin/python
"$BG_PY" -m serial.tools.list_ports -v
```

用实际串口替换下方占位符，使用 `bg status` 核对角色。拔插后重新枚举。

```sh
T01_PORT=/dev/cu.usbmodem_REPLACE_TAG
C01_PORT=/dev/cu.usbmodem_REPLACE_COUNSELOR
tag() { "$BG_PY" tools/bg_serial.py --port "$T01_PORT" command "$@"; }
c01() { "$BG_PY" tools/bg_serial.py --port "$C01_PORT" command "$@"; }
tag bg status
c01 bg status
tag bg_wifi status
c01 bg_wifi status
```

应看到 T01 `role=TAG`、C01 `role=COUNSELOR`，两端 `confirmed=1`、`wifi configured=1`。

## 1. 云端 OTA + A/B 分组（先做）

| 设备 | Cohort | 演示前版本/策略 | 演示后 |
|---|---|---|---|
| T01 | ab-a-control | 1.5.0 / A_CONTROL / 60 秒 | 保持 1.5.0 |
| C01 | ab-b-fast | 1.5.1 / B_FAST / 15 秒 | 1.5.2 / B_FAST / 15 秒 |

先打开 [Devices](https://app.memfault.com/organizations/university-of-pennsylvania-w1vt/projects/nrf-project/devices)，展示两个分组和版本。再打开 [1.5.2 Release](https://app.memfault.com/organizations/university-of-pennsylvania-w1vt/projects/nrf-project/releases/1.5.2)，展示 **Not activated** 和两种硬件的 Payloads。虽然有 T01 的兼容镜像，只向 B 组发布，所以 T01 应保持原版。

开始记录 C01，再去网页激活：

```sh
"$BG_PY" tools/bg_serial.py --port "$C01_PORT" \
  --log local/checkoff-c01-ota.log monitor --seconds 300 \
  --until 'Image health check complete; confirm result=0'
```

网页点 **Create activation → Cohort: ab-b-fast → Normal → Activate**。板子约每 30 秒检查发布。监视命令在结束时集中输出日志；保持两块板供电。应看到 downloading、百分比和 `confirm result=0`。不要把发布激活到 A 或 default。

终端返回后：

```sh
c01 bg status
tag bg status
tag bg_wifi check
```

成功标准：C01 `version=1.5.2 confirmed=1`；T01 仍为 `version=1.5.0 confirmed=1`。等自动上传后刷新 Devices，SW Version 也应更新。发布页 **Activation** 只列 `ab-b-fast`，**Target Devices** 应只有 BG-C01。若监视超时，先看 `bg status`；必要时 `c01 bg_wifi check` 并再监视，不能只凭百分比判定成功。

### 1a. A/B 的可观察结果

在执行任何 `mflt test heartbeat` 之前，保持设备正常运行至少 90 秒。分别打开 [T01 Reports](https://app.memfault.com/organizations/university-of-pennsylvania-w1vt/projects/nrf-project/devices/BG-T01?tab=reports)、[C01 Reports](https://app.memfault.com/organizations/university-of-pennsylvania-w1vt/projects/nrf-project/devices/BG-C01?tab=reports)，Date Range 选包含当前时间的窗口，刷新并展开自然生成的 heartbeat：

- T01：`bg_ab_variant=A_CONTROL`、`bg_report_interval_s=60`、`MemfaultSdkMetric_IntervalMs` 约 60000。
- C01：`bg_ab_variant=B_FAST`、`bg_report_interval_s=15`、`MemfaultSdkMetric_IntervalMs` 约 15000。
- 比较连续报告的 **End (Captured Time)**：A 约一分钟，B 约 15 秒。Wi-Fi 每约 30 秒上传，Received Time 可能批量相同。

解释给 TA：A/B 实际改变诊断采样策略，发布范围由云端 cohort 限制。当前两块板角色不同，所以不把 CPU 差异当成实验结论。C01 的 1.5.0 历史报告也是 A_CONTROL/60 秒，可展示同一设备前后变化。详见 [A/B 实验说明](FLEET_AB_TEST.md)。完成此步再做下面的手动采样。

## 2. Metrics 和供电

```sh
tag mflt test heartbeat
tag bg_wifi check
c01 mflt test heartbeat
c01 bg_wifi check
```

`mflt test heartbeat` 立即采集真实快照。打开 [C01 Reports](https://app.memfault.com/organizations/university-of-pennsylvania-w1vt/projects/nrf-project/devices/BG-C01?tab=reports)，等待上传后刷新，展开最新 heartbeat。核对软件版本与 **End / Captured Time**，再看：

| 字段 | 预期 |
|---|---|
| cpu_usage_pct | 实测 CPU 使用率，随负载变化 |
| Heap_BytesFree | 剩余堆内存 |
| memory_sysworkq_pct_max 等 | Zephyr 线程栈指标 |
| bg_image_confirmed | 1 |
| bg_usb_host_active | USB 连接电脑时为 1 |
| bg_power_status | USB_HOST_ACTIVE |

供电项只观察 USB 主机活动；不是电池百分比、VBUS 电压或充电检测。打开 [T01 Reports](https://app.memfault.com/organizations/university-of-pennsylvania-w1vt/projects/nrf-project/devices/BG-T01?tab=reports)，展示 `bg_imu_available=1` 与 `bg_motion`。C01 无 IMU，相关值为 0 正常。

## 3. 关键产品信息：失联计数

先记下 C01 最新报告的 `bg_missing_events_total=N`，再执行：

```sh
tag bg heartbeat on
tag bg maintenance 0
c01 bg maintenance 0
sleep 4
tag bg heartbeat off
sleep 14
tag bg heartbeat on
sleep 4
c01 bg radio
c01 mflt test heartbeat
c01 bg_wifi check
```

`bg heartbeat` 控制 BLE 存在广播，LoRa 仍工作。终端返回 Heartbeat off/on，C01 radio 的 replies 应体现 LoRa 回复；刷新最新 Reports 后计数从 N 增加到 N+1。计数是本次开机累计，重启/OTA 后重新从 0 开始。一定先结束 OTA 并关闭维护窗口。

## 4. Metrics 可视化

C01 页面 **Timeline → Metrics → None → 勾选 bg_missing_events_total**。时间尺度先选 15 minutes，再 **Date Range → Custom range → Show time**，设为 **实际演示时间**，覆盖操作前后并 Apply。多个采样点应体现计数变化。CPU、Heap_BytesFree 可以分别展示；字符串 bg_power_status 在 Reports 看。

## 5. Reboot reason

```sh
tag bg reboot
sleep 20
tag bg_wifi check
tag mflt get_reboot_reason
```

应看到硬件 `Current Reboot Reason Reg: 0x0008`、软件意图 `Prior Stored Reboot Reason: 0x0002`。刷新 [T01 Reboots](https://app.memfault.com/organizations/university-of-pennsylvania-w1vt/projects/nrf-project/devices/BG-T01?tab=reboots)，最新主动重启 Type 为 **User Reset**；OTA 对应 **Fw Update**。

## 6. Coredump（最后做）

```sh
tag mflt test assert
sleep 20
tag bg_wifi check
tag bg status
```

`tag` 是前面定义的 T01 命令快捷函数；`mflt test assert` 故意触发断言失败，保存崩溃现场后重启，不会擦除固件。恢复后 T01 应仍为 1.5.0、confirmed=1。打开 [T01 Traces](https://app.memfault.com/organizations/university-of-pennsylvania-w1vt/projects/nrf-project/devices/BG-T01?tab=traces)，刷新后点最新 **Assert at memfault_demo_cli_cmd_assert**，确认当前版本与时间。

向下找 **Coredump → State → Threads → shell_uart**，展示函数 `memfault_demo_cli_cmd_assert`、文件 `memfault_demo_panics.c` 第 129 行和调用栈。对应版本 ELF 已上传后，云端才能从地址解析源码位置。

若开机出现 `Time sync unavailable; HTTPS postponed`，保持联网等自动重试，再运行 `tag bg_wifi check`；不要把暂时尚未上传误判成没有生成 dump。

无需手动导出 chunks。板子会自动上传并清理本地 dump，因此 `mflt get_core` 没有本地记录时，应核对云端最新 Trace。

## 实现解释

独立 Wi-Fi worker 连接网络、对时、经 HTTPS 上传遥测并查询发布。固件分块写 secondary，完整接收并 flush 尾块后请求 MCUboot test boot；MCUboot 验签，应用健康运行至少 15 秒后确认。SDK 收集系统指标、复位原因与崩溃线程栈，项目只补充 LoRa/失联/IMU/USB 等真实业务快照。

详细实现与构建见 [Fleet 指南](FLEET_MANAGEMENT.md)，实际已验证范围见 [验证摘要](../VALIDATION.md)。
