# Fleet Management extra credit：A/B 遥测策略

## 实验内容

通过 Memfault cohort 分组发布两种诊断策略，验证只有目标组接收新版本，并比较真实的指标采集周期。

| 策略 | 编译参数 | 每份报告中的 bg_ab_variant | bg_report_interval_s |
|---|---|---|---:|
| A 对照组 | --heartbeat-seconds 60 | A_CONTROL | 60 |
| B 实验组 | --heartbeat-seconds 15 | B_FAST | 15 |

两组均保留同一套 BLE 存在广播、LoRa、告警、MCUboot 签名和回滚逻辑。Wi-Fi 上传/发布检查间隔仍约 30 秒，因此 B 的 15 秒采集不保证每 15 秒到达云端，可能一次上传多个报告。

`bg_ab_variant` 表示**正在运行的固件策略**。云端 Cohort 表示**分组分配**；仅移动设备分组不会立即改变已经运行的固件，必须完成对应 OTA。

## 对照与解释边界

本轮两块板分别是 tag 和 counselor，不能用两者的 CPU、内存或业务流量直接得出哪个策略更好。实验验证的是 cohort 定向发布和实际采集周期。C01 自身在 A、B 固件上的报告可以提供同设备前后对照；这仍不是随机、大样本或电池寿命实验。

B 每分钟名义采集 4 份，A 每分钟 1 份。以云端报告的 **End / Captured Time**、`MemfaultSdkMetric_IntervalMs` 和 `bg_report_interval_s` 验证；不要用 Received Time 推断采集周期。测量期间不要运行 `mflt test heartbeat`，它会额外生成手动报告。

## 复现构建

在已配置的 West 工作区中使用原团队私钥（仓库只提供公钥）：

```sh
BG_WS="$HOME/buddyguard-workspace"
BG_KEY="$HOME/.config/buddyguard/team2-signing/firmware-ecdsa-p256.pem"
"$BG_WS/.venv/bin/python" scripts/build.py --workspace "$BG_WS" --key "$BG_KEY" \
  --build-dir "$BG_WS/build-ab-a-c01" --role counselor --wifi \
  --heartbeat-seconds 60 --version 1.5.0
"$BG_WS/.venv/bin/python" scripts/build.py --workspace "$BG_WS" --key "$BG_KEY" \
  --build-dir "$BG_WS/build-ab-b-c01" --role counselor --wifi \
  --heartbeat-seconds 15 --version 1.5.1
```

Tag 对应 `--role tag --imu`，必须使用不同构建目录。报告 `buddyguard-build.json` 记录采集周期，防止复用另一实验配置的目录。不要使用相同 software type + version 发布不同策略的镜像。

## 云端分组和操作

1. 在 [Cohorts](https://app.memfault.com/organizations/university-of-pennsylvania-w1vt/projects/nrf-project/cohorts) 创建 `ab-a-control`、`ab-b-fast`。
2. 先让两块板运行 A 基础版本并留存自然生成的 60 秒报告。
3. 将 T01 分配到 `ab-a-control`，C01 分配到 `ab-b-fast`。
4. B Release 同时包含 tag、counselor 两种兼容硬件的 B 镜像；只对 `ab-b-fast` 做 Normal activation。这样能证明 A 没有升级是 cohort 限制，而不是缺少兼容 payload。
5. 等待 C01 Wi-Fi 更新并 confirmed。核对 T01 保持 A、C01 为 B，然后等待至少 90 秒产生自然报告。
6. 分别打开两设备 Reports，核对版本、策略、采集周期和相邻报告时间；发布页 Target Devices 应只包含 B 组设备。

## 2026-11-08 实板结果

| 实测样本（EST） | 固件 | 实际策略 | SDK 间隔 |
|---|---|---|---:|
| C01 16:05:47，更新前 | 1.5.0 | A_CONTROL | 60000 ms |
| T01 16:06:46，对照组 | 1.5.0 | A_CONTROL | 60000 ms |
| C01 16:07:38，更新后 | 1.5.1 | B_FAST | 15000 ms |

C01 自然报告连续结束时间为 16:07:23、16:07:38，间隔 15 秒，两份于 16:07:49 左右批量到达。A 的连续自然报告在 16:01:47、16:02:47、16:03:47 等时间结束。所有样本来自板子 Wi-Fi 上传，没有手动触发 heartbeat。

1.5.0 已对两组激活作为共同基线，1.5.1 包含两种硬件镜像但只对 `ab-b-fast` 激活。C01 自动更新、自确认；T01 保持已确认的 1.5.0。队友预备的演示发布为 **1.5.2 / B_FAST**，计划只向 B 组激活；T01 保持 A。详见 [checkoff 操作卡](FLEET_CHECKOFF.md)，构建/回归范围见 [验证摘要](../VALIDATION.md)。

实现只有构建配置和两个真实指标：`scripts/build.py` 覆盖 SDK heartbeat 周期，`fleet.c` 报告该配置。云端 cohort 判断由 Memfault 的设备 OTA API 完成，设备端沿用现有签名下载路径。

参考：[Memfault OTA、Cohort 与适用发布规则](https://docs.memfault.com/docs/platform/ota)。
