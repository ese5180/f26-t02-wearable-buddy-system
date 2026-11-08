# Fleet Management

[中文](../zh/FLEET_MANAGEMENT.md) · [Live checkoff commands (中文)](../zh/FLEET_CHECKOFF.md) · [A/B experiment (中文)](../zh/FLEET_AB_TEST.md) · [Validation](../VALIDATION.md)

The `--wifi` profile gives the XIAO a direct 2.4 GHz PSK Wi-Fi connection to Memfault for metrics, reboot events, coredumps and signed OTA. The normal BLE/LoRa build is available separately. `--fleet` alone enables diagnostics with a USB host gateway.

## 1. Build

Follow [Getting started](GETTING_STARTED.md) for the native toolchain and both Python environments. The manifest pins Memfault SDK 1.46.0. In an existing workspace whose manifest still points to Zephyr, add the SDK under `$BG_WS/modules/lib/` if it is missing:

```sh
git clone --depth 1 --branch 1.46.0 https://github.com/memfault/memfault-firmware-sdk.git \
  "$BG_WS/modules/lib/memfault-firmware-sdk"
```

Build from the repository root. Set `BG_WS` to your actual workspace and use the existing team signing key. These commands show the A/B profiles. Pick a fresh version and build directory for a new cloud release.

```sh
BG_WS="$HOME/buddyguard-workspace"
BG_KEY="$HOME/.config/buddyguard/team2-signing/firmware-ecdsa-p256.pem"
"$BG_WS/.venv/bin/python" scripts/build.py --workspace "$BG_WS" --key "$BG_KEY" \
  --build-dir "$BG_WS/build-bg-wifi-t01" --role tag --tag-id 1 --imu --wifi --version 1.5.0
"$BG_WS/.venv/bin/python" scripts/build.py --workspace "$BG_WS" --key "$BG_KEY" \
  --build-dir "$BG_WS/build-bg-wifi-c01" --role counselor --wifi --heartbeat-seconds 15 --version 1.5.1
```

Omit `--imu` without the LSM6DSO. Wi-Fi builds apply the checked-in Zephyr shared-heap lock patch and record its hash in the build report. Use `firmware/zephyr/zephyr.signed.bin` for OTA. Upload the matching `zephyr.elf` to Memfault under **Software → Symbol Files** for each role and build.

| Role | Device serial | Software type | Hardware version |
|---|---|---|---|
| Tag | BG-T01 / BG-T02 / BG-T03 | buddyguard-tag | xiao-esp32s3-wio-tag-v1 |
| Counselor | BG-C01 | buddyguard-counselor | xiao-esp32s3-wio-counselor-v1 |

## 2. Install and provision

For devices with Team 2 MCUboot, install each role's initial Wi-Fi image using the BLE update steps in [Getting started](GETTING_STARTED.md). Verify version, role and `confirmed=1` with `bg status`. Obtain the [BuddyGuard cloud project](https://app.memfault.com/organizations/university-of-pennsylvania-w1vt/projects/nrf-project/devices) access and its Project Key through the team.

Create the mode-600 files `$HOME/.config/buddyguard/wifi.json` and `$HOME/.config/buddyguard/memfault-project-key`. Put only the Project Key in the second file and enter the Wi-Fi password locally:

```json
{"ssid": "AirPennNet-Device", "security": "psk", "password": "REPLACE_LOCALLY"}
```

List ports with `.venv/bin/python -m serial.tools.list_ports -v` and set `TAG_PORT` and `COUNSELOR_PORT`. Register the **Wi-Fi STA MAC** for Penn's device network. Add `--status` to either command to read that MAC before provisioning:

```sh
.venv/bin/python tools/bg_wifi.py --port "$TAG_PORT" --device BG-T01
.venv/bin/python tools/bg_wifi.py --port "$COUNSELOR_PORT" --device BG-C01
```

The tool checks identity and image confirmation, saves the settings in NVS and reboots. Settings survive OTA. Lab NVS is unencrypted. The worker waits for DHCP, synchronizes time, uploads over HTTPS and checks for releases about every 30 seconds. Use `bg_wifi status` to inspect setup and `bg_wifi check` to request a check. Verify the device identity and version in **Devices → Reports**.

## 3. Release and check off

1. Create a Memfault release with the signed application for each eligible role and upload the matching symbols. Activate it for the intended cohort. Use the unconfirmed signed image. The worker flushes the completed download into secondary before requesting a test boot. Check the version and `confirmed=1` on the device and verify the new cloud version.
2. In **Reports**, check Zephyr CPU, heap and thread metrics. `bg_usb_host_active` reports observed USB activity. Battery state of charge and the actual power source are unavailable. Demonstrate the counselor's `bg_missing_events_total` and the tag's IMU status.
3. Use `bg reboot` for a User Reset. On confirmed bench firmware, trigger `mflt test assert`, let the device restart and upload, then inspect **Reboots / Traces** for the reason and symbolized stack. Follow the [checkoff card](../zh/FLEET_CHECKOFF.md) for the live sequence.

Wi-Fi and BLE SMP share secondary. Run one update path at a time. Coredumps use space reserved at the end of secondary, so upload a pending crash before a BLE update or secondary erase. Wi-Fi retries interrupted downloads from the beginning. A/B changes the diagnostic sample interval independently of the upload interval.

## 4. Optional USB diagnostics

Build and install with `--fleet` alone. Close the serial monitor before starting the gateway:

```sh
.venv/bin/python tools/bg_fleet.py --port "$TAG_PORT" --device BG-T01 \
  --key-file "$HOME/.config/buddyguard/memfault-project-key" --seconds 180
```

The gateway queues Memfault chunks from USB in local SQLite and retries failed uploads. Install updates with BLE SMP for this profile. The Wi-Fi profile uploads directly.

References: [Memfault Zephyr integration](https://docs.nrfcloud.com/docs/mcu/zephyr-guide) · [Penn device Wi-Fi](https://isc.upenn.edu/using-wireless-penn).
