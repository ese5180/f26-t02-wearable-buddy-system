# Modular firmware validation / 模块化固件验证 — 2026-10-30

Source implementation / 实现提交: `3d78fa6`. Version / 版本: **1.1.16**, protocol1. Board: `xiao_esp32s3/esp32s3/procpu`; Zephyr `f80761e49401497b5ee64ee9732dc2791055ec52`, SDK1.0.1, west1.5.0, Python3.12. Two physical ESP32S3/Wio B2B kits were used; USB provided power/logging and BLE carried the updates.

本次使用两套实板，USB 供电／记录，应用更新通过 BLE。下面记录实际观察，不将编译成功当成硬件验证。

| Check / 检查 | Observed result / 实测结果 |
|---|---|
| Native/Python protocol tests / 协议测试 | 3 unittest cases passed; C golden-vector test compiles the actual `wire.c`, checks payload layout and malformed frames / 三项通过，实际编译固件协议代码 |
| Both role builds / 双角色构建 | Tag and counselor built and verified with the team public identity / 标签和辅导员均构建、验签通过 |
| Clean clone / 干净克隆 | Local Git clone of committed files, without ignored local artifacts, built T02 with `BG-T02`, ID2; signature verified / 无本机临时文件的提交克隆成功构建 T02 |
| Profile guards / 配置保护 | Reusing tag directory for counselor and passing counselor with `--tag-id` were both rejected / 拒绝混用角色目录及无效角色参数 |
| BLE OTA / 无线更新 | Both full signed transfers completed; each started 1.1.16 unconfirmed, self-confirmed after the health window, then SMP showed active+confirmed / 两端完整上传、试运行、自确认及状态查询通过 |
| SPI / 硬件读回 | Both roles read private sync `0x1424` / 两端同步字正确 |
| Maintenance behavior / 维护宽限 | No unexpected BLE MISSING in the counselor OTA capture / 更新抓取中没有意外失联告警 |
| LoRa SOS | Counselor ACTION logged; tag ACK on attempt1 / 辅导员收到事件，标签首试获 ACK |
| BLE missing / 失联 | Stopping BLE presence produced `age_ms=8929`, then QUERY seq1 / 停广播后约 8.929 秒报警并查询 |
| LoRa QUERY/REPLY | Counselor received type5 seq1; tag got reply ACK on attempt1 / 回复及 ACK 首试成功 |
| BLE recovery / 恢复 | Fresh seq31 produced RECOVERED with maintenance0 / 新序号触发恢复 |
| Watchdog rollback / 故障回滚 | Signed 1.2.1 booted unconfirmed, injected failure, reset with `TG0WDT_SYS_RST`; returned to confirmed 1.1.16, also verified by SMP / 真实试运行故障后恢复已确认镜像 |
| Final state / 结束状态 | Both 1.1.16 confirmed, BLE disconnected, maintenance0; tag heartbeat restored on / 两端已确认、维护关闭、标签心跳已恢复 |

The final committed C sources were rebuilt after formatting; **both unsigned application SHA256s matched the original builds installed over BLE**. ECDSA signatures can change on rebuild, including their encoded length, so the exact transferred signed artifacts are preserved locally and their hashes are in [current-builds.json](current-builds.json). This is not a claim that signed files are byte-identical across builds.

格式整理后的最终源码重新构建，两端**未签名应用哈希与已刷入版本一致**。重新签名可能改变签名字节和长度；表内签名镜像对应实际上传并保留在本机的文件。旧 1.1.15 元数据保存在 [builds-v1115.json](builds-v1115.json)。

Evidence: [machine-readable summary](modularization-v1116-summary.json), [selected normalized serial lines](modularization-v1116.log), [installed build hashes](current-builds.json). Full raw captures and binaries remain ignored/local. Earlier wrong-key, corruption and interrupted-resume results remain in [the historical validation record](validation.md); those negative cases were not repeated in this refactor, which leaves MCUboot/partition configuration unchanged.

证据见上述摘要、日志片段与镜像哈希。错误密钥／损坏拒绝、中断续传沿用历史实测，本次未重复；MCUboot 和分区配置没有更改。本次新做了故障回滚，验证拆分后的 watchdog／确认路径。

Limits: the clean-clone build reused the existing pinned SDK/workspace; a fresh macOS installation was documented, not performed. T02 was compiled, not physically exercised; there is still only one physical tag in this regression. Existing partition macros emit two deprecation warnings on this pinned revision. New backup/provisioning guard paths were reviewed and CLI-checked; destructive initial USB provisioning was not repeated on healthy installed boards. Occasional STATUS messages still exhaust three attempts; QUERY still lacks retry and ACK waits can consume other frames. This does not establish field range, child pairing, sensor integration, authorized management or production safety.

边界：干净克隆复用了现有固定 SDK／工作区，没有重装整台 Mac；T02 只编译，未实板测试。固定版本的旧分区宏仍有两条弃用提示。新备份／首次安装保护做了检查和 CLI 验证，没有对正常板重复整片擦除。STATUS 偶尔仍三试未确认，QUERY 无自动重试，ACK 等待可能丢其他帧；不代表户外范围、儿童配对、外设、管理授权或产品安全已完成。
