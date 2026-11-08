# BuddyGuard concept and prototype scope

The team's October 11, 2026 proposal described an offline group-awareness system for phone-free summer camps. Campers wear tags and counselors use a handheld to monitor the group. Camp operators are the intended purchasers, with Greater Philadelphia day camps as the proposed starting market. The proposal records teaching-team feedback on group monitoring, pairing, motion data, and privacy. Camp outreach and interviews were listed as planned or in progress.

## Design decisions

| Area | Proposal | Bench prototype |
| --- | --- | --- |
| Group presence | Default group monitoring with optional buddy pairs for trips and hikes | The counselor tracks BLE presence by tag ID. The bench has one tag and one counselor. Buddy pairing is planned. |
| Hardware | Nordic-based tags and an nRF7002 DK handheld | Both roles use the XIAO ESP32S3 and Wio-SX1262 B2B kit. A SparkFun LSM6DSO was tested on the tag over I2C. |
| Loss and response | Local notice, then a longer-range alert or roll call | The counselor detects missed BLE heartbeats and requests status via LoRa. The tag can send SOS from a shell command. |
| Motion and peripherals | Motion context, clasp sensing, vibration and battery status | LSM6DSO reports UNKNOWN, STILL or MOVING. Clasp, haptics and battery sensing remain outside the tested build. |
| Cloud | The handheld would relay tag diagnostics and updates | Each Wi-Fi-enabled device reports directly to Memfault and downloads its own signed release. BLE SMP remains available for nearby updates. |
| Security and privacy | Signed firmware, session-based identifiers and authenticated LoRa | MCUboot verifies application signatures. Static lab IDs are used. Business-frame authentication and session ID rotation remain planned. |

The heartbeat represents device communication. The prototype uses loss of BLE contact and LoRa replies, with no location fix or measured separation distance. Production radio range, battery life, market sizing and pricing were proposal targets or estimates. See the [MVP BOM](https://docs.google.com/spreadsheets/d/1GG7O95_0aSs89AQl4g6phD0VFDLTcqFAaMB5zflHOEA/edit?gid=2071228825#gid=2071228825), the [project log](PROJECT_LOG.md) and the [measured bench results](VALIDATION.md).
