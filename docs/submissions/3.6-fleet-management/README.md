# BuddyGuard

**Team Number: 02**

**Team Name: Jet2Holiday**

| Team Member Name | Email Address |
| --- | --- |
| Siwei Lei | <laialex@engineering.upenn.edu> |
| Rico Zhuang | <zzhuan13@engineering.upenn.edu> |
| Yunzhe Deng | <deng1@engineering.upenn.edu> |
| Yubin Guan | <guan1@engineering.upenn.edu> |

**GitHub Repository URL:** <https://github.com/ese5180/f26-t02-wearable-buddy-system>

**Fleet Management code:** [Pull Request #1](https://github.com/ese5180/f26-t02-wearable-buddy-system/pull/1) · [README at commit `37ac4ee`](https://github.com/ese5180/f26-t02-wearable-buddy-system/blob/37ac4eea06b825bb6f3397d22ac2596482048b19/README.md)

## 3.6 Fleet Management

We tested the fleet setup with one tag (T01) and one counselor unit (C01). Both run Zephyr on XIAO ESP32S3 boards with Wio-SX1262. Each board connects directly to Memfault over Wi-Fi for diagnostics and firmware updates.

### OTA and version information

We activate a signed release for a cohort in Memfault. The board downloads its image over HTTPS into the secondary flash slot. MCUboot checks the signature on reboot, and the application confirms the image after a healthy trial. The new firmware version then appears on the device page in Memfault.

### Coredumps and reboot reasons

We trigger an assertion with `mflt test assert`. After reboot, the board uploads the coredump and Memfault uses the matching ELF to show the call stack. The Reboots tab shows events such as User Reset, Assert, and Firmware Update.

### Metrics

- **System:** Zephyr CPU usage, free heap, and thread stack usage.
- **Power:** USB host activity detected from USB frames. Battery state of charge is unavailable.
- **BuddyGuard:** We pause T01's BLE broadcasts and check that C01's `bg_missing_events_total` increases in the next report.

### Extra Credit: A/B Rollout

We released version 1.5.1 to the B cohort. C01 upgraded and collected a report every 15 seconds. T01 stayed on 1.5.0 and collected a report every 60 seconds. We compared capture times because the boards can upload several reports together.

[Build and setup](https://github.com/ese5180/f26-t02-wearable-buddy-system/blob/37ac4eea06b825bb6f3397d22ac2596482048b19/docs/en/FLEET_MANAGEMENT.md) · [Bench results](https://github.com/ese5180/f26-t02-wearable-buddy-system/blob/37ac4eea06b825bb6f3397d22ac2596482048b19/docs/VALIDATION.md)
