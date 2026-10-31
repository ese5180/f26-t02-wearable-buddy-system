# LSM6DSO hardware validation / IMU 实板验证 — 2026-10-31

Implementation / 实现提交: `d9c7f99`. Physical pair / 实板：**T01 1.1.17 + C01 1.1.18**, same protocol1. Source VERSION is now1.1.18. IMU first shipped in1.1.17; the counselor-only scanner correction followed in1.1.18. Signed application upgrades used BLE; USB supplied power and logs. MCUboot/partitions/signing identity were unchanged.

[中文 IMU 操作](../docs/zh/IMU_LSM6DSO.md) · [English IMU guide](../docs/en/IMU_LSM6DSO.md) · [Structured results](imu-lsm6dso-20261031-summary.json) · [Selected logs](imu-lsm6dso-20261031.log) · [Exact installed hashes](current-builds.json)

## Measurements and transport / 测量与传输

| Check / 检查 | Observed result / 实测 |
|---|---|
| Hardware / 硬件 | SparkFun SEN-18020 LSM6DSO; 3V3/GND, SDA=D4 GPIO5, SCL=D5 GPIO6 |
| Detection / 识别 | `address=0x6b`, `WHO_AM_I=0x6c`; actual sensor identified / 已读到真实芯片 |
| Six axes / 六轴 | Example at rest: accel `(-66,8,1007)` mg, gyro `(5,-4,-2)` mrad/s / 静置实测示例 |
| Sampling / 采样 | Sample count22289, age11 ms, error0 in final snapshot; approximately24 valid samples/s on this bench / 样本持续增长，数据新鲜 |
| Real motion / 真实晃动 | User gently moved/rotated, then rested the sensor. Tag recorded STILL→MOVING→STILL / 由用户实际操作，非注入数据 |
| LoRa motion / 运动传输 | C01 v1.1.17 received STATUS seq5 MOVING, then seq6 STILL; tag received ACKs / 收到对应状态与 ACK |
| SOS and query / 告警与查询 | SOS type3 and manual QUERY REPLY type5 carried STILL; counselor ACTION logged / 原有链路带上真实状态 |
| Optional hardware / 可选硬件 | C01 `bg imu` reports enabled0/available0; no IMU required on counselor / 辅导员不需传感器 |
| Signed updates / 签名更新 | Both v17 roles self-confirmed; C01 then BLE-upgraded to v18 and self-confirmed. Final SMP queries show expected primary versions active+confirmed / 最终版本与确认状态已查询 |
| Native tests / 本机测试 | Four Python/native unittest cases passed, including actual C motion logic: gravity, warm-up, activity hold, stale sampling and clock wrap / 四项通过 |
| Reproduction / 复现 | Clean Git clone of d9c7f99 built with `--imu --version 1.1.17`; signature verification passed. Existing pinned SDK/workspace reused / 干净提交克隆与隔离版本构建通过 |

The v17 sensor build and v18 counselor build came from the working tree committed as d9c7f99; the only subsequent implementation change was counselor scanning. The tag remained on its working v17 image because the scanner correction compiles only for the counselor. This is not a claim of byte-identical rebuilds; version/build metadata and ECDSA signatures can change. Exact transferred-image hashes are recorded separately.

传感器 v17 与辅导员 v18 的实现已提交为 d9c7f99；后续代码变化仅涉及辅导员扫描。标签保持已正常工作的 v17，不需要为辅导员修改再次升级。复现通过指能够构建、验签；不宣称不同构建的文件逐字节一致。实际上传文件的哈希见独立记录。

## Scanner regression and correction / 扫描去重修正

While the tag continued valid sampling and advertising in normal mode, C01 v17 emitted unintended BLE MISSING/RECOVERED events. The separate normal-presence capture also contained an unintended missing event. The convenience `BT_LE_SCAN_PASSIVE` macro enables controller duplicate filtering. The roster needs changing heartbeat sequences from the same address; application-level sequence checks already reject stale advertisements.

标签正常采样／广播时，v17 辅导员仍出现意外失联／恢复。检查发现扫描便利宏默认开启控制器去重，而名单需要持续接收同一地址变化的心跳序号。因此 v18 显式使用 `BT_LE_SCAN_OPT_NONE`，保留原有扫描窗口／间隔、8 秒失联阈值和应用层序号去重。

After the v18 update, the first normal ONLINE was at C01 uptime00:00:06.124 with maintenance0. The last valid heartbeat before the deliberate stop was at00:03:12.327, derived from the later warning timestamp and reported age. This gives **186.203 seconds of normal observation without an unintended missing alarm**, with IMU sampling active. This bounded before/after result supports the scanner correction; it does not establish field reliability.

v18 更新后，正常 ONLINE 为00:00:06.124，故意停止前最后有效心跳为00:03:12.327（由报警时间减去 age 得到），因此有 **186.203 秒正常观察无意外失联**，期间 IMU 持续采样。该前后结果支持扫描配置修正，不代表户外可靠性已验证。

Then `bg heartbeat off` deliberately stopped BLE only. C01 reported `age_ms=9896`, sent QUERY seq1 and received a type5 reply with motion=STILL. Restoring heartbeat produced RECOVERED at00:03:34.229. There was exactly one MISSING in the v18 capture, corresponding to this deliberate test. Thus the short alarm was retained rather than hidden by a larger timeout.

随后故意关闭标签 BLE 心跳，C01 在 `age_ms=9896` 报失联，QUERY seq1 获得含 STILL 的 REPLY；恢复心跳后00:03:34.229 输出 RECOVERED。v18 抓取中唯一一次 MISSING 对应该故意测试，未通过增大超时隐藏告警。

## Handoff and limits / 交接与边界

Final state: T01 v1.1.17 with IMU available, heartbeat on, maintenance0, confirmed; C01 v1.1.18 with maintenance0, confirmed. All host monitors/clients were closed. The latest signed files stay local, public verification key remains in Git, private keys and full-flash backups remain excluded. Earlier v16 metadata is preserved in [builds-v1116.json](builds-v1116.json).

结束状态：标签 v17 传感器有效、心跳开启、维护关闭、已确认；辅导员 v18 维护关闭、已确认。电脑端监视器／客户端已关闭，私钥和整片备份仍不上传。

Scope: one IMU, one tag and one counselor; heuristic motion thresholds; no fall/danger diagnosis or location. Physical IMU disconnection/power-failure was not tested; native tests cover classifier gaps and freshness/error paths were reviewed. QUERY still lacks automatic retry and ACK waits can consume other frames. Occasional STATUS retry exhaustion remains possible. Management/business-frame security, sensors other than this IMU, buddy pairing and Wi-Fi/cloud remain outside this bench stage. Earlier rollback/wrong-key/corruption evidence is retained; those tests were not repeated for this optional-sensor addition.

范围：仅一颗 IMU、一标签及一辅导员；阈值为实验启发式，不判断跌倒／危险或位置。未做 IMU 实体断线／断电测试；本机测试覆盖采样间断，过期／错误路径做了代码检查。QUERY 自动重试、ACK 等待期间接收、业务认证、其他外设、儿童配对及 Wi-Fi／云仍待完善。旧安全负例／回滚记录保留，本次未重复。
