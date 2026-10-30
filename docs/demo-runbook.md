# BuddyGuard bench demo

**2026-10-30:** both devices run confirmed 1.1.15 after complete BLE updates; the maintenance-grace and normal loss/recovery tests passed. Use the verified results in [validation.md](../evidence/validation.md).

Use two assembled ESP32S3 + Wio-SX1262 B2B kits with the correct LoRa and BLE antennas attached. The current devices are T01 (tag) and C01 (counselor). Keep antennas apart on the bench. USB supplies power and the shell; application traffic uses BLE/LoRa, and updates use BLE.

## Local paths and ports

Repository: `/Users/guan/esp/projects/buddyguard`.

Current Mac ports on 2026-10-30, which may change after reconnection:

| Device | Port | BLE name |
|---|---|---|
| T01 | `/dev/cu.usbmodem101` | `BG-T01` |
| C01 | `/dev/cu.usbmodem1101` | `BG-C01` |

Run commands from the repository with `.venv/bin/python`. Use only one serial reader per port and one BLE management client per device.

## Presence, LoRa and alarm demonstration

1. Run `tools/bg_serial.py --port PORT command bg status` for each device. Verify the intended version (1.1.15), correct role and `confirmed=1`. Do not assume a candidate is active merely because its upload completed.
2. Close maintenance admission on both devices: `tools/bg_serial.py --port PORT command bg maintenance 0`. This makes the short loss-detection demo possible; maintenance otherwise grants a 600-second grace period in 1.1.15.
3. Monitor C01: `tools/bg_serial.py --port /dev/cu.usbmodem1101 monitor --seconds 45`. Expect periodic LoRa `RX ... type=1` status messages. In a separate terminal monitor T01 to see matching `ACK ... type=1` messages.
4. Stop the T01 serial monitor before using its shell. Run `tools/bg_serial.py --port /dev/cu.usbmodem101 command bg sos`. C01 should print `type=3` and `ACTION: counselor check tag T01, SOS received`. The tag should get an ACK.
5. With C01 still monitored, run `tools/bg_serial.py --port /dev/cu.usbmodem101 command bg heartbeat off`. After about 8–10 seconds C01 should report `BLE MISSING`, send a LoRa QUERY and receive a `type=5` reply. This means BLE contact was lost while the tag remained reachable over LoRa; it is not a measured separation distance.
6. Restore BLE with `tools/bg_serial.py --port /dev/cu.usbmodem101 command bg heartbeat on`. C01 should report `RECOVERED` on a fresh sequence.
7. To request a manual roll call, stop C01's serial monitor and run `tools/bg_serial.py --port /dev/cu.usbmodem1101 command bg query 1`. If no type=5 reply appears, retry after the periodic STATUS exchange. QUERY has no automatic retry in this prototype; one overlap with tag TX was observed during testing.

LoRa types: STATUS=1, ACK=2, SOS=3, QUERY=4, REPLY=5. Each tag event gets up to three delivery attempts. A query response is logged separately and does not erase a missing-BLE warning. RSSI/SNR describe the received radio signal; they are not coordinates.

## Signed BLE update demonstration

Build from the pinned Zephyr workspace, using the private team key stored outside the repository:

```sh
/Users/guan/zephyrproject/.venv/bin/python scripts/build.py --build-dir /Users/guan/zephyrproject/build-bg-demo --version 1.1.16
# Add --counselor when building for C01.
```

Use separate build directories for roles and fault cases. Never transfer a counselor-role image to a tag or a tag-role image to the counselor; the lab bootloader trusts the common team key and does not enforce role matching.

Open a maintenance window on the target, then monitor its serial port in another terminal:

```sh
.venv/bin/python tools/bg_serial.py --port /dev/cu.usbmodem101 command bg maintenance 300
.venv/bin/python tools/bg_ble.py images
.venv/bin/python tools/bg_ble.py update /Users/guan/zephyrproject/build-bg-demo/firmware/zephyr/zephyr.signed.bin
```

For C01, select its USB port and add `--name BG-C01` before `images`/`update`. The default 244-byte SMP packet limit avoids BLE reassembly during the bench demo. A roughly 435 KB upload can take several minutes. A transport timeout triggers reconnection and SHA-based resume; keep power on. The client requests a test boot, not permanent confirmation. Wait for `Image health check complete; confirm result=0`, then query `images` again and verify the intended version is active and confirmed.

To demonstrate interruption before activation:

```sh
.venv/bin/python tools/bg_ble.py upload /path/to/same/zephyr.signed.bin --stop-at 20
.venv/bin/python tools/bg_ble.py images
.venv/bin/python tools/bg_ble.py update /path/to/same/zephyr.signed.bin
```

Expect the first transfer to exit intentionally at 20%, with the old primary still confirmed. The second transfer should report its first resumed offset near 20%. Uploading only part of a file never requests activation.

To build a correctly signed rollback test, use `--fault --version 1.2.0` in a separate build directory. First verify a known-good image is confirmed, then transfer the fault image over BLE. Expect the 1.2.0 banner, `FAULT INJECTION`, a watchdog reset and a return to the prior confirmed version. This deliberate fault is never a release image.

The development Mac also retains `artifacts/wrong-key-1.3.0.signed.bin` and `artifacts/corrupt-1.4.0.signed.bin` for the two rejection demos. Transfer one with the same `update` command, allow the test-reset to finish, and verify that the prior confirmed version is still active. An SMP `bootable` flag or successful upload is not proof of signature acceptance; verify the version after reboot. These files are intentionally invalid and are excluded from Git.

## Before checkoff

Keep the two known-good role images, private key and full-flash backups in their existing local storage. Reopen maintenance only when needed, restore `bg heartbeat on` after experiments, and check both devices report confirmed. The course's interpretation of external Flash and choice of device-management platform still require teaching-team confirmation. Wi-Fi/cloud and the additional product sensors are outside this bench stage.
