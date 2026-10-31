# BuddyGuard: teammate reproduction guide

[Optional SparkFun LSM6DSO / IMU guide](IMU_LSM6DSO.md)

[中文](../zh/GETTING_STARTED.md) · [Firmware explanation](FIRMWARE.md) · [Validation record](../../evidence/validation.md)

Target: XIAO ESP32S3 + Wio-SX1262 **B2B kit**, using Zephyr and MCUboot. This guide covers the current BLE/LoRa/signed-update bench prototype. Start with **one tag (T01) + one counselor (C01)**. The software accepts T01–T03, but a complete four-device kit has not been validated. No sensor breakout, GPS, battery or display is required for this demo; USB supplies power.

## 1. Hardware preparation

Disconnect USB, stack the Wio board onto the XIAO and fully seat the **underside board-to-board connector**. Merely placing the boards beside each other does not connect SPI. Attach the 915 MHz antenna to Wio and the 2.4 GHz antenna to XIAO. Reconnect each XIAO through a USB **data** port/cable; a hub's PD input can supply power without exposing a serial port. Keep antennas clear of metal and separated on the bench.

No jumper wires are needed for the B2B kit. Do not use the through-hole nRF52840 pinout. Pin assignments are in [FIRMWARE.md](FIRMWARE.md#hardware-and-radio-settings). See the [official kit assembly guide](https://wiki.seeedstudio.com/wio_sx1262_with_xiao_esp32s3_kit/) for the physical arrangement.

## 2. Create a clean development workspace

The validated host is Apple Silicon macOS with Python 3.12, west 1.5.0 and Zephyr SDK 1.0.1. Other operating systems need their own USB/BLE setup and have not been tested by this team. Install Homebrew first if absent. Keep **all source, build, workspace and key paths free of spaces**.

```sh
xcode-select --install  # once; skip if command-line tools are installed
brew install cmake ninja gperf ccache dtc wget python@3.12
BG_WS="$HOME/buddyguard-workspace"
mkdir -p "$BG_WS"
cd "$BG_WS"
python3.12 -m venv .venv
source .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install west==1.5.0
git clone --branch codex/buddyguard-wireless-fota https://github.com/ese5180/f26-t02-wearable-buddy-system.git buddyguard
west init -l buddyguard
west update
west zephyr-export
west packages pip --install
west blobs fetch hal_espressif
west sdk install --version 1.0.1 --gnu-toolchains xtensa-espressif_esp32s3_zephyr-elf
cd buddyguard
python3.12 -m venv .venv
.venv/bin/python -m pip install -r requirements-host.txt
.venv/bin/python -m unittest discover -s tests -v
```

The **workspace `.venv`** runs west, esptool, imgtool and provisioning. The **repository `.venv`** runs BLE and serial host tools. Use the explicit executable paths below; activating one environment is not enough to provide both dependency sets. `west.yml` pins Zephyr to `f80761e49401497b5ee64ee9732dc2791055ec52` (a 4.4.99 development checkout). `west update` resolves its imported module revisions. Fetch **all** hal_espressif blobs: the Bluetooth configuration checks more than only ESP32S3 blobs. The ordinary BLE/LoRa build requires no Zephyr source patch. Optional UART recovery applies a narrow MCUboot Kconfig prerequisite automatically; see [UART recovery](UART_RECOVERY.md). The other software-SPI patch in `patches/` is historical and must not be applied.

If reusing an existing workspace, set `BG_WS` to it, confirm the pinned revision and supply `--workspace "$BG_WS"`. Do not run `west init` a second time. SDK installation needs internet/download time and disk space. [Zephyr's host installation guide](https://docs.zephyrproject.org/latest/develop/getting_started/index.html) and [board reference](https://docs.zephyrproject.org/latest/boards/seeed/xiao_esp32s3/doc/index.html) provide host-specific details; this repository's pinned revision takes precedence over newer examples.

## 3. Signing identity

For the **existing Team 2 boards**, obtain the existing team development signing key through the team's controlled private channel, or obtain signed images from the signing workstation. Git contains only `keys/team2-signing-public.pem`. Put the private key outside the checkout:

```sh
BG_KEY="$HOME/.config/buddyguard/team2-signing/firmware-ecdsa-p256.pem"
chmod 700 "$(dirname "$BG_KEY")"
chmod 600 "$BG_KEY"
```

These commands assume the privately supplied file already exists. Do not paste the key into chat, commit it, or copy it into the source tree. A different key cannot update the bootloader already installed on a team board.

For **new, unprovisioned development boards only**, a separate lab root can be generated and used for both initial provisioning and all subsequent updates on those boards:

```sh
mkdir -p "$HOME/.config/buddyguard/my-lab"
chmod 700 "$HOME/.config/buddyguard/my-lab"
BG_KEY="$HOME/.config/buddyguard/my-lab/firmware-ecdsa-p256.pem"
# Only run keygen when this path does not already exist. Preserve that key afterward.
"$BG_WS/.venv/bin/imgtool" keygen -k "$BG_KEY" -t ecdsa-p256
chmod 600 "$BG_KEY"
```

This creates a different device identity; it does not make an image trusted by existing team boards. Never overwrite an established signing key.

## 4. Build the correct role

From the repository, with `BG_WS` and `BG_KEY` set:

```sh
"$BG_WS/.venv/bin/python" scripts/build.py --workspace "$BG_WS" --key "$BG_KEY" --build-dir "$BG_WS/build-bg-tag1" --role tag --tag-id 1
"$BG_WS/.venv/bin/python" scripts/build.py --workspace "$BG_WS" --key "$BG_KEY" --build-dir "$BG_WS/build-bg-counselor" --role counselor
```

T02/T03 use `--tag-id 2` / `--tag-id 3` and **separate build directories**. Names are BG-T01/BG-T02/BG-T03 and BG-C01; counselor ID is 200. `--counselor` is retained as an older alias. Role, observer mode, ID and fault setting are explicitly generated on every build. A profile guard rejects reuse of a directory with another role/ID/key/fault setting. Choose a new directory when it rejects one; do not manually mix generated configuration files.

Outputs in each build directory:

| Path | Purpose |
|---|---|
| `firmware/zephyr/zephyr.signed.bin` | Signed application for BLE updates |
| `mcuboot/zephyr/zephyr.bin` | Initial USB bootloader only |
| `buddyguard-build.json` | Role, version, pinned revision, sizes and hashes |
| `signing-public.pem` | Public key derived from the selected signing identity |

The script verifies the signed image after building. Keep tag and counselor artifacts distinguishable: **the common signing root does not enforce hardware role matching**. The bootloader trusts signatures, not the filename or role.

## 5. Find the USB ports

```sh
.venv/bin/python -m serial.tools.list_ports -v
```

Set the two ports for this session; these are examples, not guaranteed names:

```sh
TAG_PORT=/dev/cu.usbmodem101
COUNSELOR_PORT=/dev/cu.usbmodem1101
```

On macOS, grant Bluetooth permission to the terminal/host process when requested. Permit only **one serial reader per port** and one BLE management connection per device. Close Arduino/other serial monitors before using these tools.

## 6. First USB provisioning — NEW boards only

Skip this section if the board already has the Team 2 MCUboot application and responds to `bg status`; use BLE updates in section 8. Initial provisioning **erases the full flash**, so first create a verified, device-specific 8 MiB backup. The backup script only reads flash but resets the board to enter/leave its USB loader. Save backups outside Git and never overwrite the original.

```sh
"$BG_WS/.venv/bin/python" scripts/backup_flash.py --port "$TAG_PORT" --output-dir "$HOME/buddyguard-backups/tag1-original"
"$BG_WS/.venv/bin/python" scripts/flash_initial.py --build-dir "$BG_WS/build-bg-tag1" --port "$TAG_PORT" --backup-info "$HOME/buddyguard-backups/tag1-original/backup-info.json" --log-prefix "$HOME/buddyguard-logs/tag1-initial"
```

Repeat with **another backup directory**, the counselor port and `build-bg-counselor`. The tools verify backup size/hash, compare device identity when present, and validate generated image addresses. Keep original `.bin` plus `backup-info.json` together. Provisioning captures startup; expect radio sync word `0x1424`, advertising and `confirm result=0`. If USB reset fails, use the board's BOOT/RESET procedure described in the [board reference](https://docs.zephyrproject.org/latest/boards/seeed/xiao_esp32s3/doc/index.html#programming-and-debugging), rediscover the port and retry without deleting the backup. Do not burn eFuses.

## 7. Two-board wireless demo

Check both devices; expect the intended version, role and `confirmed=1`:

```sh
.venv/bin/python tools/bg_serial.py --port "$TAG_PORT" command bg status
.venv/bin/python tools/bg_serial.py --port "$COUNSELOR_PORT" command bg status
.venv/bin/python tools/bg_serial.py --port "$TAG_PORT" command bg maintenance 0
.venv/bin/python tools/bg_serial.py --port "$COUNSELOR_PORT" command bg maintenance 0
```

Allow a fresh tag advertisement to reach the counselor. In a **second terminal**, set the counselor port again and start:

```sh
.venv/bin/python tools/bg_serial.py --port "$COUNSELOR_PORT" --log evidence/raw/my-wireless-demo.log monitor --seconds 90
```

In the first terminal (the tag port is free):

```sh
.venv/bin/python tools/bg_serial.py --port "$TAG_PORT" command bg sos
.venv/bin/python tools/bg_serial.py --port "$TAG_PORT" command bg heartbeat off
# Wait about 12 seconds, then restore presence:
.venv/bin/python tools/bg_serial.py --port "$TAG_PORT" command bg heartbeat on
```

Expected counselor sequence: periodic `RX ... type=1` STATUS; SOS `type=3` and `ACTION`; after about 8–10 seconds, `BLE MISSING` → `QUERY` → `RX ... type=5`; then `RECOVERED` on fresh BLE. LoRa replies do **not** clear BLE-missing status. If a query collides with periodic tag TX, retry `bg query 1` on C01 **after closing its serial monitor**. QUERY has no automatic retry yet. A queued SOS is not proof of delivery; inspect counselor reception and tag ACK.

`heartbeat off` simulates loss of **communication heartbeat**, not pulse. It leaves LoRa running. This demo does not measure child separation or location. Motion is UNKNOWN without `--imu`; the optional LSM6DSO guide describes real motion sensing. Clasp remains UNKNOWN and battery unavailable.

## 8. Signed update over BLE

Use a distinct candidate version/build directory, e.g. `--version 1.1.19`, adding the **same role/ID as the target**. Version override copies source into an isolated generated directory; it does not edit the checkout's VERSION. Open maintenance, let the counselor receive its maintenance flag (wait about 4 seconds), then update:

```sh
"$BG_WS/.venv/bin/python" scripts/build.py --workspace "$BG_WS" --key "$BG_KEY" --build-dir "$BG_WS/build-bg-tag1-candidate" --role tag --tag-id 1 --version 1.1.19
.venv/bin/python tools/bg_serial.py --port "$TAG_PORT" command bg maintenance 300
# Wait about 4 seconds before connecting:
.venv/bin/python tools/bg_ble.py --name BG-T01 images
.venv/bin/python tools/bg_ble.py --name BG-T01 update "$BG_WS/build-bg-tag1-candidate/firmware/zephyr/zephyr.signed.bin"
```

The last path assumes you first built that candidate in `build-bg-tag1-candidate`. Default SMP packet size is 244 bytes; allow several minutes for a ~435 KB image. Keep power on. The client reconnects/resumes the **same file** after transport failures and requests a **test boot**, never permanent host confirmation. Monitor the target's serial port while uploading if desired. Expect the new version, then `Image health check complete; confirm result=0` after at least 15 healthy seconds. Run `bg status` and `images` again: **the intended version must be active and confirmed**. Reopen maintenance after expiry to query images. For C01 use `--name BG-C01`, its USB port, and the counselor artifact.

A tag's advertisements pause while a BLE SMP client is connected. If C01 last saw its maintenance flag, it uses a **600-second grace** instead of the normal 8-second timeout. Grace is bounded; a long/stalled update can still trigger missing status. Admission closes after 300 seconds, but an already-connected upload may finish.

Optional reproduction of secure-update checks:

1. **Interruption/resume:** run `upload IMAGE --stop-at 20`, verify the old image remains confirmed, then `update IMAGE` with exactly the same file. Expect a nonzero resumed offset.
2. **Watchdog rollback:** first confirm a known-good image. Build another version in a separate directory with `--fault --version 1.2.1` and the correct role/ID. Update it. Expect the fault banner, WDT reset and restoration of the previously confirmed image. Never use a fault build as a release.
3. **Wrong key/corruption:** historical completed BLE rejection tests are in the validation record. To repeat, use a separate external test key or a copied image with corrupted signed data; verify the old confirmed version after reboot. Never replace the trusted team key or its known-good artifacts.

## 9. Troubleshooting and handoff

| Symptom | Action |
|---|---|
| LED on, no serial port | Check USB data cable/hub port; power alone is insufficient |
| Readback `0xffff` instead of `0x1424` | Power off and reseat B2B; check control pin mapping; keep upstream GPIO definitions |
| `west`/`sdk` command absent | Run west from the initialized workspace using its `.venv/bin/west` |
| Build fails on ESP blob checks | Run `west blobs fetch hal_espressif` in the pinned workspace |
| Build profile mismatch | New directory for each role/ID/fault/signing identity |
| BLE device not connectable | `bg maintenance 300`, Bluetooth permissions, close other BLE clients |
| No missing alarm after 8 seconds | Close maintenance and let C01 see a fresh normal tag frame first |
| Upload finished but version unchanged | Inspect MCUboot/serial/image state; upload is not signature acceptance |
| Occasional LoRa unconfirmed/QUERY timeout | Retry manual query; bench retry logic is not a reliable safety channel |

Before handing off, restore `bg heartbeat on`, check both roles and `confirmed=1`, record Git commit and `buddyguard-build.json`, and save normalized evidence without secrets. Close serial/BLE tools. Core source/docs/public key belong in Git; private keys, raw flash backups, `.venv`, builds, signed artifacts and raw captures stay local. See [firmware limitations](FIRMWARE.md#implemented-scope-and-limits) before describing the demo as a complete product.
