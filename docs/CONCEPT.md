# 3.8 Concept Refinement

**BuddyGuard · ESE 5180 Team 02 · Jet2Holiday**

## 3.8.1 Concept Review & Refresh

### Description

Counselors at day camps use headcounts to supervise children during transitions, trips and outdoor activities. BuddyGuard adds a local alert when a camper's tag stops checking in. The first prototype covers one tag and one counselor. Camp operators rent a kit of three tags and one counselor unit for a quarter.

### Product Function

A tag broadcasts a BLE heartbeat, and the counselor unit marks it missing when the heartbeat times out. The unit then requests a LoRa status reply, and a tag can also send a LoRa SOS. At base, each board can use Wi-Fi to report device health and receive signed updates through Memfault.

### Target Market & Demographics

Counselors operate the system and campers wear the tags. The buyers are camp directors, municipal recreation departments and organizations running summer programs. Our first market is Philadelphia day camps serving school-age children, starting with sites that run field trips or frequently move groups between activities.

The ACA reports about **20,000 U.S. camps**, including day and overnight programs. Philadelphia Parks & Recreation reports **more than 100 neighborhood day camps** and **more than 7,000 campers** across its summer camps. The estimate assumes one rented quarter per site per year. [ACA camp data](https://www.acacamps.org/sites/default/files/2025-07/ACA-Camp-Stabilization-Toolkit-2025.pdf) · [Philadelphia Parks & Recreation](https://www.phila.gov/services/culture-recreation/activities-and-events/register-for-summer-camp/)

| Market | Assumption | Annual rental revenue |
| --- | --- | ---: |
| U.S. upper-bound TAM | 20,000 camps × 5 kits × $199 | $19.9M |
| Philadelphia starting market | 100 city day camps × 5 kits × $199 | $99,500 |
| First-season target | 2 pilot sites × 5 kits × $199 | $1,990 |

Five kits per site and $199 per kit per quarter are assumptions. Figures exclude shipping and assume a single summer rental period. Interviews will test uptake, kit quantities and price. [Team cost model](https://docs.google.com/spreadsheets/d/1Vo3cIDeer3M5wfd5n27pI2ez8do8EjMCnAkfwn7qf3Q/edit?gid=2020998309#gid=2020998309)

### Competitors and Alternatives

| Alternative | What it does well | Where BuddyGuard fits |
| --- | --- | --- |
| [Campminder / Campanion](https://campminder.com/features/transportation/) | Attendance check-ins, bus rosters and pickup coordination. | Adds a local warning between check-ins. A camp can keep its existing attendance system. |
| [Garmin Bounce 2](https://www.garmin.com/en-US/newsroom/press-release/sports-fitness/parents-can-delay-the-smartphone-with-bounce-2-kids-smartwatch-from-garmin/) | Individual location tracking and communication for families. Announced at $299.99 per watch plus $9.99/month for LTE service. | A counselor can monitor the group from one local unit. BLE and LoRa supervision run without cellular service or GPS collection. |
| [Nelos](https://nelos.app/) | Phone-based LoRa group messaging and GPS tracking outside cell coverage. | Uses a dedicated counselor unit and detects missed presence heartbeats without collecting coordinates. Nelos is better suited to users who need a map and a last-known location. |

**Our advantage is a phone-free group workflow.** A counselor can get an automatic missing-tag alert and request a LoRa reply without internet access. The reply can add motion context when an IMU is fitted. Staff still use their camp procedures to locate and account for campers.

### Stakeholders

| Potential stakeholder | Who to interview | What to validate |
| --- | --- | --- |
| [Greater Philadelphia YMCA Day Camps](https://www.philaymca.org/camps/summer-day-camp/) | Camp directors at West Philadelphia or Roxborough, plus a lead counselor | How staff count campers during trips, who receives an alert, and what false alarm rate is acceptable. |
| [Philadelphia Parks & Recreation](https://www.phila.gov/services/culture-recreation/activities-and-events/register-for-summer-camp/) | Neighborhood day camp managers and recreation center supervisors | Group size, charging and handoff procedures, and site approval for a pilot. |
| [Philadelphia Summer Achievers](https://www.phila.gov/2026-05-08-register-now-make-this-summer-unforgettable-with-summer-achievers-summer-camp-at-edey-schools-2/) | City program coordinators and host-site operators | Whether the system fits field-trip staffing, which organization buys or rents equipment, and what data it will accept. |

These are outreach targets. Interview a site director and a lead counselor about alerts, quarter length and returns.

### System-Level Diagrams

The device block diagram and communication diagram are deferred. The tag and counselor unit use BLE for presence and 915 MHz LoRa P2P for queries and alerts. Each unit can reach Memfault over Wi-Fi at base. The optional tag IMU connects over I²C. [Firmware architecture](en/FIRMWARE.md)

### Security Requirements Specification

**Overview.** Protect firmware integrity and limit the data exposed by supervision and diagnostics.

**Definitions.** OTA is an over-the-air update. A coredump is a snapshot collected after a crash.

| ID | Requirement | Verification |
| --- | --- | --- |
| SEC 01 | MCUboot shall reject application images without a valid signature from the configured Team 2 ECDSA P-256 key. | Load a valid image and a wrong-key image, then compare boot results. Bench-tested over BLE. |
| SEC 02 | The baseline system shall omit camper names and coordinates from BLE frames, LoRa frames and cloud reports. | Inspect representative frames, report schemas and diagnostic payloads before a pilot. |
| SEC 03 | The device shall validate the HTTPS certificate and hostname before sending diagnostics or accepting a Wi-Fi OTA download. | Inspect the TLS configuration and test with an untrusted certificate. Negative TLS test pending. |
| SEC 04 | Before a pilot, LoRa receivers shall reject modified and replayed alerts. | Modify and replay captured frames. Pending. |
| SEC 05 | Uploaded coredumps shall exclude roster and key material. | Audit crash capture regions before a pilot. Pending. |

### Hardware Requirements Specification

**Overview.** The tag and counselor unit share an ESP32S3/LoRa platform. The tag can add motion sensing.

**Definitions.** B2B is a board-to-board connector. IMU is an inertial measurement unit. LiPo is a lithium-polymer battery.

| ID | Requirement | Verification |
| --- | --- | --- |
| HRS 01 | Each unit shall use the XIAO ESP32S3 and Wio-SX1262 B2B kit, with separate 2.4 GHz and 915 MHz antennas. | Inspect assembly and read the SX1262 sync word over SPI. Bench-tested. |
| HRS 02 | A motion-enabled tag shall use the SparkFun LSM6DSO on 3.3 V I²C. | Confirm WHO_AM_I `0x6C` and sample changes on movement. Bench-tested. |
| HRS 03 | The production tag shall operate for at least 10 hours on one charge in supervision mode. | Measure a complete duty cycle and run a discharge test. Pending. |
| HRS 04 | The pilot tag shall have a wearable enclosure and a physical SOS control. | Check fit and verify that a button press sends SOS. Pending. |
| HRS 05 | The pilot counselor unit shall have a display that shows the affected tag ID. | Trigger a missing-tag event and check the display. Pending. |

### Software Requirements Specification

**Overview.** Zephyr runs the tag and counselor profiles, the radio services and the optional Fleet Management worker.

**Users.** Camp counselors use presence and alerts. Team or camp fleet administrators manage diagnostics and updates.

**Definitions.** A heartbeat is a BLE presence message. A LoRa ACK acknowledges a received message. UNKNOWN means that a usable motion sample is unavailable.

| ID | Requirement | Verification |
| --- | --- | --- |
| SRS 01 | The tag shall advance its BLE heartbeat sequence at least once every 3 seconds in normal supervision mode. | Inspect fresh sequence numbers during a bench run. |
| SRS 02 | The counselor shall mark a previously seen tag missing within 11 seconds of its last fresh heartbeat in the normal bench profile, then queue one LoRa query. | Stop the tag broadcast and time the MISSING event. The configured timeout is 8 seconds plus roster polling. |
| SRS 03 | Each LoRa transaction shall send at most three times and report success or exhaustion. | Test with a responding and an absent peer. |
| SRS 04 | A motion-enabled tag shall report STILL or MOVING from fresh samples and UNKNOWN when the sample is unavailable or more than 1 second old. | Move the IMU, hold it still, then disconnect it or force stale data. |
| SRS 05 | A complete signed OTA image shall stage in secondary flash and run as a trial. An unconfirmed image that watchdog-resets shall revert to the last confirmed image. | Run a healthy update and a signed fault-injection update. |
| SRS 06 | In the Wi-Fi profile, each unit shall upload its firmware version, reboot reasons, crash data and operational metrics to Memfault. | Compare device output with the device page, reports and traces. |

See the [bench record](VALIDATION.md) for which cases were actually run.

## 3.8.2 Power Budget

Use the BOM's 400 mAh LiPo and assume 80% usable capacity. A rough supervision-mode estimate is **90 mA** with Wi-Fi off, based on 85 mA for the BLE-active XIAO, about 4.6 mA for LoRa receive and 0.55 mA for the IMU. This first-pass sum treats the component currents as equivalent battery-side loads. The estimate gives **320 mAh ÷ 90 mA ≈ 3.6 hours** before accounting for LoRa transmit bursts and other loads. The 10-hour target requires an average of **32 mA or less** on this pack. Measure current at the battery with a PPK2, then evaluate duty-cycled receive or a larger pack. [Seeed board data](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/) · [Semtech radio data](https://www.semtech.com/products/wireless-rf/lora-connect/sx1262) · [ST IMU data](https://www.st.com/en/mems-and-sensors/lsm6dso.html)

## 3.8.3 Hardware BOM

The model covers two device types in a four-device rental kit. It includes radios, antennas, batteries, controls, cases, straps, fasteners, charging and packaging. The sheet totals **$228.09 at one kit** and **$195.34 per kit at 1,000 kits**. It still includes three optional GNSS modules and the earlier Adafruit IMU. For a no-GNSS kit using the tested SparkFun IMU on all three tags, the same line-item assumptions give about **$213.12 / $178.72**. The latter uses SparkFun's published 100+ tier and needs a volume quote. [Hardware BOM](https://docs.google.com/spreadsheets/d/1Vo3cIDeer3M5wfd5n27pI2ez8do8EjMCnAkfwn7qf3Q/edit?gid=2071228825#gid=2071228825) · [SparkFun pricing](https://www.sparkfun.com/sparkfun-6-degrees-of-freedom-breakout-lsm6dso-qwiic.html)

## 3.8.4 Software BOM

The sheet budgets **$3,495/month** for Memfault Growth and **$10/month** for a proposed AWS backend, or **$42,060/year**. Growth lists 1,000 included reporting devices, equivalent to 250 fully active kits. The prototype has no separate AWS backend. Confirm a quote for the number of reporting devices, whether OTA is included, and what backend the product needs. Local LoRa P2P requires no cellular or LoRaWAN subscription. [Software BOM](https://docs.google.com/spreadsheets/d/1Vo3cIDeer3M5wfd5n27pI2ez8do8EjMCnAkfwn7qf3Q/edit?gid=1647301774#gid=1647301774) · [Memfault pricing](https://memfault.com/pricing/)

## 3.8.5 Financial Model

Rent each kit to the camp for **$199 per quarter plus $15 shipping**. Start with two Philadelphia sites and ten kits for one summer quarter. At return, inspect, recharge, reflash and replace worn straps before reuse.

The spreadsheet's U.S. rental scenario uses 3,000 kits and one rented season per year. It models a **$426,827 first-year loss** and **$606,901 profit over three years** before the separate recurring software budget. That scenario illustrates the upfront inventory cost and the effect of reusing kits. Use the pilot to check rental utilization, refurbishment cost and willingness to renew before buying a larger fleet. [Rental scenario](https://docs.google.com/spreadsheets/d/1Vo3cIDeer3M5wfd5n27pI2ez8do8EjMCnAkfwn7qf3Q/edit?gid=1482974775#gid=1482974775) · [Cost inputs](https://docs.google.com/spreadsheets/d/1Vo3cIDeer3M5wfd5n27pI2ez8do8EjMCnAkfwn7qf3Q/edit?gid=2020998309#gid=2020998309)

## Submission Check

| Part | Check before submission |
| --- | --- |
| 3.8.1 Concept | Text and appendix-format requirements are in this document. Add the two diagrams when ready. |
| 3.8.2 Power | Enter the current and runtime assumptions in the workbook and replace them with measurements when available. |
| 3.8.3 Hardware BOM | Separate the GNSS option and update the IMU and enclosure costs. |
| 3.8.4 Software BOM | Verify the cloud plan and price for the active fleet, including OTA. |
| 3.8.5 Financial model | Use the quarterly rental scenario, include recurring software costs and check teaching-team access to the workbook. |
