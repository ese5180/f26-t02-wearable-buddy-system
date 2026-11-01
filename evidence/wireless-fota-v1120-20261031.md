# Wireless reliability + BLE FOTA regression — 2026-10-31

[中文复现](../docs/zh/WIRELESS_RELIABILITY.md) · [English runbook](../docs/en/WIRELESS_RELIABILITY.md) · [Source learning map / 源码入口](../firmware/README.md) · [Structured results](wireless-fota-v1120-20261031-summary.json) · [Selected logs](wireless-fota-v1120-20261031.log)

**Final state / 最终状态：T01 and C01 run active/confirmed1.1.20, heartbeat enabled, connected0, maintenance0.** Both were updated through BLE. External TTL UART was disconnected; native USB supplied power/logs/commands. Installed bootloaders were not rewritten. C01 retained its earlier63,728-byte UART-recovery MCUboot; T01 retained its46,672-byte bootloader.

Firmware implementation: `0c3a612`; test-first policy specification: `78918e8`; guarded host cleanup: `93b9199`. Zephyr remains pinned to `f80761e49401497b5ee64ee9732dc2791055ec52`. The actual radio policy C source is compiled by host tests. **18 local tests passed**; this is not a completed Zephyr Ztest/CI/HIL assignment.

## Module changes / 模块修改

`radio/exchange.c` owns deadlines, at most three sends and response matching without hardware or sleeps. `radio/history.c` retains sixteen recent business identities. `radio/link.c` remains the sole radio hardware worker and dispatches all valid packets while transactions wait. QUERY now waits for a same-tag/same-sequence REPLY and retries; other events wait for ACK. ACK byte5 optionally identifies its source message type while legacy zero ACKs remain accepted. The existing200 ms synchronous RX window is retained. New `bg radio` counters make scheduling outcomes visible.

重试、去重、硬件驱动、名单、传感器与健康确认分别维护职责。`main.c` 保留启动编排；新增外设应提供快照，不要在 BLE 回调或 shell 中直接操作 LoRa。详见学习与复现文档。

## BLE security regression on the installed recovery MCUboot / 新bootloader下无线回归

| Test / 测试 | Observed result / 实测结果 |
|---|---|
| Valid C01 upgrade / 正常辅导员更新 | Full435,899-byte BLE transfer in210.95 s;1.1.20 actually booted unconfirmed, healthy confirmation at uptime20.128 s |
| Valid T01 upgrade / 正常标签更新 | Full437,099-byte BLE transfer in218.51 s;1.1.20 booted unconfirmed, healthy confirmation at uptime20.125 s; real LSM6DSO0x6b/WHO0x6c sampling continued |
| Maintenance grace / 更新维护宽限 | No unintended BLE-missing event during the complete tag update; C01 continued receiving LoRa status |
| Wrong signing identity / 错误签名根 |1.3.2 validated under its own test root, failed the team root, completed216.73 s BLE upload/test/reset; did not boot; original1.1.20 digest remained active/confirmed |
| Corrupted signed payload / 损坏镜像 | Byte0x400 of a copied1.4.1 image was flipped; offline verification rejected its hash; full222.62 s BLE upload/test/reset did not activate it; original primary retained |
| Controlled interruption/resume / 受控中断续传 | Validly signed fault image1.2.2 paused at87,258 bytes; original primary remained active/confirmed. The identical file resumed from87,258; remaining transfer took170.78 s |
| Valid signature, faulty code / 有效签名但故障代码 |1.2.2 actually booted unconfirmed, printed fault injection, stopped feeding WDT; `rst:0x7 (TG0WDT_SYS_RST)` followed by confirmed1.1.20 startup |
| Inactive test candidate cleanup / 清理未激活候选 | C01 secondary was erased after confirmed-primary/no-pending checks. BLE erase ACK was lost; separate readback verified unchanged primary and absent secondary. A subsequent CLI invocation confirmed no secondary image was listed |

The cleanup helper now reconnects for readback without repeating an erase when its ACK is lost; that lost-ACK branch also has an asynchronous unit regression. No negative-test bootloader was installed. The restored image's active digest is checked, not inferred from upload acknowledgement or `bootable` alone. UART recovery itself was not re-tested in this round; [earlier same-day UART evidence](uart-recovery-20261031.md) remains valid for that recorded run.

## Radio behavior / 无线行为

- While querying absent T02/T03, C01 still dispatched/ACKed T01 SOS and STATUS. Each absent query exhausted at three sends rather than blocking forever. This was repeated with both endpoints on1.1.20.
- In normal mode, intentionally stopping only T01 BLE advertisements triggered missing at **age8,619 ms**. Automatic QUERY seq4 succeeded on **attempt2**, returned real STILL motion, and received its ACK. Restoring BLE produced `RECOVERED ... seq=50`.
- Other valid packets were dispatched while pending queries waited; the physical interleaving observation is on C01. Tag-side query deferral shares the nonblocking worker/policy but was not forced with deterministic RF injection.
- IMU samples were real; no motion, clasp, battery or location values were simulated. Clasp/battery remain unknown/unavailable.

## Five-minute normal-operation check / 五分钟正常模式检查

A **302.353-second** run after all firmware tests closed maintenance and left both boards broadcasting normally. It issued20 T01 queries,8 T01 SOS requests and one deliberately unanswered T02 query.

| Observation | Result |
|---|---:|
| T01 queries completed |20/20 |
| SOS counselor ACTION events |8/8 |
| Unexpected BLE missing events |0 |
| Unexpected device resets |0 |
| Request-queue drops |0 |
| Deliberate absent-T02 transaction |3 sends, then1 exhausted result |
| C01 transaction attempts / completed |31 /20 |
| T01 transaction attempts / completed |79 /58 |
| T01 exhausted transactions during this interval |0 |
| C01 other packets dispatched while waiting |11 |

`tx_attempts` counts scheduled transaction sends, not immediate ACK transmissions. These results include retries; they do **not** prove a loss-free radio channel. T01's58 completed transactions include STATUS, SOS and REPLY. A full uninterrupted30-minute run, outdoor range/power work and three-tag concurrency were not performed.

## Learning and reproduction notes / 学习与复现注意

An early superseded candidate was interrupted around30% before activation. Reconnecting before its old BLE client fully exited caused SMP sequence mismatches. Both clients were closed; a fresh query verified original1.1.19 remained active/confirmed. The final200 ms RX-window image was transferred as a new SHA. The later controlled87,258-byte same-file resume above is the successful resume proof. These transient connection errors are not counted as signature rejection.

The synchronous radio driver sleeps the modem on receive timeout; the final implementation retains the established200 ms window. A shorter diagnostic window was not installed. Remaining half-duplex blind periods and RF loss still need measurement. Recent-history duplicate suppression is bounded and can confuse a rebooted sender reusing recent sequences until eviction; a session-aware protocol is future work.

Current source does not implement child-to-child pairing, all additional peripherals, cloud management or management/business-message authentication. The team confirmed no course Memfault/nRF Cloud project exists yet; see [next stage / 下一阶段](../docs/zh/NEXT_STEPS.md). Independent teammate-machine reproduction, course external-Flash interpretation and Gradescope/checkoff remain outstanding. Private keys, Flash backups, generated binaries and raw captures stay local; only selected evidence is committed.
