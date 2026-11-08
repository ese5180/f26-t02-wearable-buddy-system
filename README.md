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

BuddyGuard helps camp counselors notice when a camper falls behind or may need help. Campers wear screen-free tags. The bench prototype uses BLE presence and LoRa P2P queries and SOS without a phone, Wi-Fi or cellular service in the field. GPS, vibration and possible-fall alerts are planned for the wearable. Wi-Fi at base handles diagnostics and signed updates. See the [refined concept](docs/CONCEPT.md).

## Start here

- [Concept and rental model](docs/CONCEPT.md) distinguishes the tested bench demo, the paid pilot kit and the commercial group kit.
- [Project stages and deliverables](docs/PROJECT_LOG.md) covers sections 3.1 through 3.13 and links to the available work.
- [Documentation index](docs/README.md) lists the English and Chinese setup, firmware and demo guides.
- [Firmware source map](firmware/README.md) links each feature to its code and build configuration.
- [Bench validation](docs/VALIDATION.md) records the exact versions and tests for one tag and one counselor.

## 3.6 Fleet Management submission

[English README](docs/submissions/3.6-fleet-management/README.md) · [PDF export](docs/submissions/3.6-fleet-management/README.pdf)

The submission covers Memfault version reporting, signed Wi-Fi OTA, coredumps, reboot reasons, Zephyr and BuddyGuard metrics, USB host activity, and the 60 s versus 15 s A/B rollout. Start with the [English Fleet guide](docs/en/FLEET_MANAGEMENT.md) or [中文操作指南](docs/zh/FLEET_MANAGEMENT.md) to reproduce it.

## Build and run

Follow [Getting started](docs/en/GETTING_STARTED.md) or [中文复现](docs/zh/GETTING_STARTED.md) to set up native macOS, Python 3.12, west 1.5.0 and Zephyr SDK 1.0.1. The board target is `xiao_esp32s3/esp32s3/procpu`, with dependencies pinned in [west.yml](west.yml).

| Build profile | What it enables |
| --- | --- |
| `--role tag` / `--role counselor` | BLE presence and signed BLE SMP updates, LoRa P2P and role-specific application behavior |
| `--imu` | Optional LSM6DSO on a tag |
| `--wifi` | Memfault diagnostics and signed HTTPS OTA |
| `--fleet` | Memfault diagnostics with a USB host gateway when used without `--wifi` |
| `--serial-recovery` | Optional MCUboot recovery over UART |

Build and sign with [scripts/build.py](scripts/build.py). Existing team boards use the Team 2 signing identity. Runtime Wi-Fi credentials and private signing keys are kept outside the repository. See the setup guides before provisioning a board.

## Repository layout

| Path | Contents |
| --- | --- |
| [firmware/](firmware/) | Zephyr application, board configuration and MCUboot configuration |
| [scripts/](scripts/) | Build, flash backup, initial provisioning and recovery bootloader install |
| [tools/](tools/) | BLE update, serial console, Wi-Fi provisioning, USB telemetry gateway and UART recovery |
| [docs/](docs/) | Project timeline, setup and implementation guides, validation and submissions |
| [keys/](keys/) and [patches/](patches/) | Team public verification key and dependency patches |

The current bench scope is one tag and one counselor. USB host activity is a power-related observation. Battery state of charge, complete multi-tag testing and outdoor validation are still open. See the [project log](docs/PROJECT_LOG.md) for the planned work.
