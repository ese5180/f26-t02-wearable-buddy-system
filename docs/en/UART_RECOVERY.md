# MCUboot UART recovery — Team 2

[中文](../zh/UART_RECOVERY.md) · [Security checkoff](FIRMWARE_SECURITY_CHECKOFF.md)

This is an optional **MCUboot-level UART** path, separate from BLE FOTA and the ESP ROM/esptool loader. Use one counselor board for the demo. Keep its full backup, original team signing key and a valid counselor image available locally. UART recovery writes the **primary** application slot; an interrupted or invalid upload can leave no running application. The bootloader remains recoverable over UART.

## 1. Connect and identify

Power the XIAO by its own USB-C. Use a **3.3V logic** USB–TTL adapter:

| Adapter signal | XIAO ESP32S3 |
|---|---|
| TX | D7 / RX / GPIO44 |
| RX | D6 / TX / GPIO43 |
| GND | GND |
| VCC, DTR, RTS | Leave disconnected |

Our unbranded CP2102 cable was tested with green=TX, white=RX, black=GND; red was disconnected. **Colors do not establish another adapter's pinout or logic voltage.** USB D+/D− are not UART TX/RX. The TTL port and native XIAO USB port must be different. On the validation Mac they were `/dev/cu.usbserial-0001` and `/dev/cu.usbmodem11301` respectively; rediscover after reconnecting:

```sh
.venv/bin/python -m serial.tools.list_ports -v
```

Use the existing workspace from the reproduction guide, with source/build/key paths without spaces. Set machine-specific paths before continuing:

```sh
BG_WS="$HOME/buddyguard-workspace"
BG_KEY="$HOME/.config/buddyguard/team2-signing/firmware-ecdsa-p256.pem"
BG_BUILD="$BG_WS/build-bg-c01-recovery"
BG_USB=/dev/cu.usbmodem11301
BG_TTL=/dev/cu.usbserial-0001
```

## 2. Build the signed application and recovery bootloader

From the repository root:

```sh
"$BG_WS/.venv/bin/python" scripts/build.py --workspace "$BG_WS" --key "$BG_KEY" \
  --build-dir "$BG_BUILD" --role counselor --serial-recovery --version 1.1.19
```

Use a fresh build directory when switching role, signing identity or recovery profile. `--version` creates an isolated source snapshot; it does not change `firmware/VERSION`. The helper verifies the signed application and records sizes/hashes in `buddyguard-build.json`.

**Pinned-version prerequisite:** the helper idempotently applies [the MCUboot threading patch](../../patches/mcuboot-esp32-uart-threading.patch) to the workspace's `bootloader/mcuboot`. It adds a first Kconfig default enabling multithreading for ESP32 serial recovery. The pinned Xtensa single-thread MCUboot received UART bytes in its FIFO but did not service the interrupt-driven serial adapter. Setting the hidden `CONFIG_MULTITHREADING=y` alone was insufficient. The helper checks the resulting symbol and refuses a bootloader larger than the unchanged 64 KiB partition. Ordinary builds without serial recovery retain the upstream threading default. No temporary diagnostic source changes are required.

The final recovery build is **63,728 bytes**, leaving **1,808 bytes** in that partition. The earlier 60,624-byte feasibility build lacked this prerequisite. The configuration selects UART0, a nominal three-second entry window, no-application recovery and primary signature validation. USB logging remains separate. The UART client allows 15 seconds for image-list replies because MCUboot computes slot hashes synchronously.

## 3. Back up the exact target before installation

Close all native-USB monitors. Choose new directories outside the repository; these commands refuse to overwrite previous evidence:

```sh
BG_BACKUP="$HOME/buddyguard-backups/c01-before-recovery"
BG_INSTALL="$HOME/buddyguard-backups/c01-recovery-install"
"$BG_WS/.venv/bin/python" scripts/backup_flash.py --port "$BG_USB" \
  --output-dir "$BG_BACKUP" --baud 115200
"$BG_WS/.venv/bin/python" scripts/install_recovery_bootloader.py \
  --build-dir "$BG_BUILD" --backup-info "$BG_BACKUP/backup-info.json" \
  --port "$BG_USB" --output-dir "$BG_INSTALL"
```

The second command is a dry run. Check that it reports `preflight: passed`, then repeat with **`--write`**. It verifies the complete 8 MiB backup hash and USB identity, existing team public root, ESP image header, partition limit and signature/UART configuration. It writes only the bootloader at `0x0`, reads the entire Flash back and requires both `bootloader_matches` and `remaining_flash_preserved` to be true. All bytes at or above `0x10000` must match the backup. Default backup baud is115200; a higher rate failed with this hub during validation.

**Do not use `flash_initial.py` here:** that is an erase-all first-provisioning flow. A normal BLE application update does not install MCUboot.

## 4. Enter recovery and query it

In one terminal:

```sh
.venv/bin/python tools/bg_uart_recovery.py --port "$BG_TTL" --wait 60 images
```

Press **RESET once** on the target while the helper is waiting. Do not hold BOOT/GPIO0: that enters the ESP ROM downloader. Alternatively, while the application still works, use a second terminal:

```sh
.venv/bin/python tools/bg_serial.py --port "$BG_USB" command bg reboot
```

The successful helper prints slot versions/hashes and `recovery_connected`. A valid SMP command keeps MCUboot in recovery until reset. With no command, it proceeds to the existing application after its entry window. Native USB's `bg` shell is unavailable while MCUboot is serving recovery. Use only one reader per port.

## 5. Upload a correctly signed counselor image

While MCUboot is in recovery:

```sh
.venv/bin/python tools/bg_uart_recovery.py --port "$BG_TTL" --wait 60 \
  upload "$BG_BUILD/firmware/zephyr/zephyr.signed.bin" --reset
```

The helper verifies against `keys/team2-signing-public.pem` before writing, prints progress, lists the uploaded image and requests reset. It sends no BLE-style test/confirm request: this path writes primary directly. Observe USB boot output and then query:

```sh
.venv/bin/python tools/bg_serial.py --port "$BG_USB" monitor --seconds 35
.venv/bin/python tools/bg_serial.py --port "$BG_USB" command bg status
```

Require the expected version, `COUNSELOR`, a healthy LoRa initialization (`0x1424` readback), BLE startup and final `confirmed=1`. This bench recovery booted already confirmed; it is not a BLE test/revert transaction and does not establish a new delayed-confirmation trial. Confirm the observed outcome rather than inferring success from an upload acknowledgement.

## 6. Demonstrate wrong-key rejection, then restore

Make a separate throwaway signing key **outside Git**, without replacing the team key:

```sh
BG_BAD_KEY="$HOME/.config/buddyguard/team2-signing/wrong-key-test.pem"
# Run keygen only if this test path does not already exist.
"$BG_WS/.venv/bin/imgtool" keygen -k "$BG_BAD_KEY" -t ecdsa-p256
chmod 600 "$BG_BAD_KEY"
"$BG_WS/.venv/bin/python" scripts/build.py --workspace "$BG_WS" --key "$BG_BAD_KEY" \
  --build-dir "$BG_WS/build-bg-c01-wrong-key" --role counselor --version 1.3.1
```

**Never install that build's MCUboot.** Use only its signed application for the negative test. Its build verifies against its own test public key; verification against the team public root must fail.

Start the following helper and reset the target to enter recovery again:

```sh
.venv/bin/python tools/bg_uart_recovery.py --port "$BG_TTL" --wait 60 \
  upload "$BG_WS/build-bg-c01-wrong-key/firmware/zephyr/zephyr.signed.bin" \
  --allow-invalid-test --reset
```

This explicit negative-test flag bypasses the host's team-root rejection and overwrites primary. Record the USB boot outcome: version1.3.1 must **not execute**. Wait beyond the normal application startup time, then query recovery again. If no valid application is available, `BOOT_SERIAL_NO_APPLICATION` keeps MCUboot recovery available. An image-list hash or successful upload does not itself mean the signature was accepted.

Restore the good image with the command in step5, reset and verify version/health/confirmation again. If MCUboot remains in no-application recovery, restoration needs no working BLE firmware. Preserve the result before disconnecting the board. Finish with `bg maintenance 0` after all BLE checks; reconnect the tag to repeat the optional two-device LoRa/IMU test.

## Limits

UART primary overwrite does not preserve BLE FOTA's secondary-slot staging guarantee. There is no UART peer authentication, no anti-downgrade policy, and ESP secure-boot eFuses/Flash encryption remain disabled. This demo proves signed application enforcement by MCUboot, not a physically locked production boot chain. Keep full Flash captures and private keys outside Git; commit only code, public verification material and curated test evidence.

Protocol reference: [MCUboot serial recovery](https://docs.mcuboot.com/serial_recovery.html).
