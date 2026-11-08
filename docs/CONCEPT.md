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

## 3.8 Concept Refinement

### 3.8.1 Concept Review & Refresh

#### Description

A counselor may count everyone before leaving the playground and have no way to tell that someone fell behind until the next count. BuddyGuard gives the counselor an earlier reason to check. Camps rent the equipment by the quarter, with three wearable tags and one counselor unit per kit.

#### Product Function

The tags check in over BLE. If one stops checking in, the counselor unit flags it and asks for a status update over LoRa. The tag uses an IMU to report movement, a GPS receiver to supply a location during an incident, and a vibration motor to get the wearer's attention. GPS is for a counselor's location request or an SOS. Wi-Fi at base handles device diagnostics and signed updates.

We tested BLE, LoRa, IMU motion and cloud updates with one tag and one counselor. GPS reporting and vibration alerts are in the MVP BOM and still need integration. [Bench results](VALIDATION.md)

#### Target Market & Demographics

Campers wear the tags. Counselors carry the receiver. The buyer is the camp operator, typically a camp director or recreation department. We're starting with Philadelphia day camps for school-age children, particularly programs that take field trips or move groups between sites.

The American Camp Association counts about **20,000 U.S. camps** across day and overnight programs. Philadelphia Parks & Recreation runs **more than 100 neighborhood day camps** and serves **more than 7,000 campers** across its summer camps. [ACA camp data](https://www.acacamps.org/sites/default/files/2025-07/ACA-Camp-Stabilization-Toolkit-2025.pdf) · [Philadelphia Parks & Recreation](https://www.phila.gov/services/culture-recreation/activities-and-events/register-for-summer-camp/)

| Market | Calculation | Rental revenue |
| --- | --- | ---: |
| **TAM** · U.S. camp market, upper bound | 20,000 camps × 5 kits × $199 | **$19.9 million/year** |
| **SAM** · Philadelphia neighborhood day camps | 100 sites × 5 kits × $199 | **$99,500/year** |
| **SOM** · First summer target | 2 sites × 5 kits × $199 | **$1,990** |

We assume five kits per site at $199 per kit for one summer quarter each year, before shipping. The counts come from published data. The price, kits per site and first-summer target are our assumptions. [Rental model](https://docs.google.com/spreadsheets/d/1Vo3cIDeer3M5wfd5n27pI2ez8do8EjMCnAkfwn7qf3Q/edit?gid=1482974775#gid=1482974775)

#### Competitors

- **Campminder / Campanion** helps staff record attendance and manage bus rosters. BuddyGuard can flag a missed check-in between headcounts, while the camp keeps its attendance system. [Campminder transportation](https://campminder.com/features/transportation/)
- **Garmin Bounce 2** gives families live location and two-way calling through an LTE service. Garmin announced it at $299.99 per watch and $9.99 per month for LTE. BuddyGuard gives camp staff one receiver for the group, with no phone or cellular plan required for local alerts. [Garmin announcement](https://www.garmin.com/en-US/newsroom/press-release/sports-fitness/parents-can-delay-the-smartphone-with-bounce-2-kids-smartwatch-from-garmin/)
- **Nelos** already offers GPS tracking and group messaging over LoRa without cell service, using a phone paired with a radio. BuddyGuard uses a dedicated counselor unit that first tells staff which tag stopped checking in, then lets them request a location if needed. [Nelos](https://nelos.app/)

#### Stakeholders

We want to speak with a director and a lead counselor at a [Greater Philadelphia YMCA day camp](https://www.philaymca.org/camps/summer-day-camp/) or a [Philadelphia Parks & Recreation site](https://www.phila.gov/services/culture-recreation/activities-and-events/register-for-summer-camp/). We also want to talk to a [Summer Achievers host-site operator](https://www.phila.gov/2026-05-08-register-now-make-this-summer-unforgettable-with-summer-achievers-summer-camp-at-edey-schools-2/) who organizes trips and summer activities. We would ask when they count campers, what they'd do after an alert, and whether quarterly rental fits their budget and return process. Interviews are planned.

#### System-Level Diagrams

The device block and communication diagrams will be added later. The tag uses I²C for the IMU, UART for GPS and a driver for the vibration motor. BLE carries presence, LoRa carries local queries and alerts, and Wi-Fi connects each device to Memfault at base. [Firmware guide](en/FIRMWARE.md)

#### Security Requirements Specification

**Overview.** Protect firmware updates and keep camper location within the assigned group.

**Definitions.** OTA means over-the-air update. A coredump is a snapshot of device memory taken after a crash.

**Functionality**

- **SEC 01.** MCUboot shall reject images that fail verification against the Team 2 ECDSA P-256 public key. Test with a valid image and a wrong-key image.
- **SEC 02.** The tag shall send GPS coordinates only to its assigned counselor unit over an authenticated and encrypted LoRa link, in response to a location request or SOS. Capture traffic during routine supervision and an incident.
- **SEC 03.** Cloud reports and coredumps shall exclude camper names, coordinates and private keys. Audit the report schema and crash capture regions before a pilot.
- **SEC 04.** Devices shall reject tampered or replayed incident messages. Modify and replay a captured packet to test this.
- **SEC 05.** Devices shall validate the HTTPS certificate and hostname before uploading diagnostics or downloading firmware. Test with an untrusted certificate.

Firmware signature checks have been tested. The GPS data path and radio security still need verification.

#### Hardware Requirements Specification

**Overview.** Both roles use the same ESP32S3/LoRa kit. The tag adds motion, location and vibration hardware.

**Definitions.** B2B means board-to-board. IMU means inertial measurement unit. GNSS is satellite positioning, including GPS.

**Functionality**

- **HRS 01.** Each unit shall use the XIAO ESP32S3 and Wio-SX1262 B2B kit with 2.4 GHz and 915 MHz antennas. Check assembly and read back the LoRa sync word over SPI.
- **HRS 02.** Each tag shall read a 6-axis IMU over 3.3 V I²C. We tested the SparkFun LSM6DSO. Verify the device ID and check the readings while moving the tag.
- **HRS 03.** Each tag shall read position fixes from a Seeed L76K GNSS module over UART. Verify a valid fix outdoors while LoRa remains operational.
- **HRS 04.** Each tag shall drive its coin vibration motor through a driver. Trigger an alert and verify that the motor runs and stops without resetting the tag.
- **HRS 05.** Each pilot tag shall fit a wrist strap and have a physical SOS control. The counselor unit shall have a display for the affected tag ID. Check fit and run a button-to-display test.
- **HRS 06.** The production tag shall run for at least 10 hours in supervision mode on one charge. Verify with a battery discharge test.

#### Software Requirements Specification

**Overview.** Zephyr runs the tag and counselor firmware. The counselor tracks presence and sends LoRa queries. The Wi-Fi worker handles cloud reporting and updates.

**Users.** Camp counselors use alerts and location requests. Fleet administrators manage devices and firmware.

**Definitions.** A heartbeat is a BLE presence message. A GPS fix is a position with a recorded time. UNKNOWN means no usable motion sample is available.

**Functionality**

- **SRS 01.** The tag shall advance its BLE heartbeat sequence at least once every 3 seconds in normal supervision mode. Check the sequence in captured frames.
- **SRS 02.** In the bench profile, the counselor shall flag a previously seen tag within 11 seconds of its last fresh heartbeat and queue a LoRa query. Stop BLE broadcasts and time the alert.
- **SRS 03.** A LoRa transaction shall make at most three attempts and report completion or exhaustion. Test with a responding and an absent peer.
- **SRS 04.** The tag shall report STILL or MOVING from fresh IMU data and UNKNOWN if the sample is more than 1 second old or unavailable. Test moving, still and stale readings.
- **SRS 05.** On a location request or SOS, the tag shall report a fix and its age if available. Send the alert immediately even when the receiver has no fix. Test with and without satellite reception.
- **SRS 06.** The tag shall vibrate after receiving a local alert command from its assigned counselor. Send the command and verify the vibration.
- **SRS 07.** Signed OTA images shall stage in secondary flash and start as trial images. A watchdog reset before confirmation shall restore the last confirmed image. Test with a signed fault image.
- **SRS 08.** In the Wi-Fi profile, each unit shall upload version, reboot reason, crash data and operational metrics to Memfault. Compare the device logs with the cloud records.

### 3.8.2 Power Budget

The BOM uses a 400 mAh LiPo. With 80% usable capacity, the tag has about 320 mAh. The first-pass estimates below use vendor currents as approximate battery-side loads, with Wi-Fi off.

| Tag mode | Approximate draw | Estimated runtime |
| --- | ---: | ---: |
| BLE active, LoRa receiving, IMU on, GPS in standby | 90 mA | 3.6 h |
| Same conditions with GPS acquiring or tracking continuously | 131 mA | 2.4 h |

The L76K adds about 41 mA when active. The motor draws about 60 mA at 3 V while vibrating. Neither estimate includes LoRa transmit bursts or vibration duty cycle. A 10-hour day needs a measured average of **32 mA or less** on this battery. We will measure the assembled tag and test GPS standby between incidents. [XIAO](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/) · [SX1262](https://www.semtech.com/products/wireless-rf/lora-connect/sx1262) · [LSM6DSO](https://www.st.com/en/mems-and-sensors/lsm6dso.html) · [L76K](https://wiki.seeedstudio.com/get_start_l76k_gnss/) · [Vibration motor](https://www.adafruit.com/product/1201)

### 3.8.3 Hardware BOM

The rental kit has three tags with GPS, an IMU and a motor driver and vibration motor on each, plus one counselor unit. The workbook includes batteries, enclosures, straps, fasteners, charging and packaging. It totals **$228.09 for one kit** and **$195.34 per kit at 1,000 kits** using the listed Adafruit IMU. Substituting the bench-tested SparkFun IMU on all three tags would bring those estimates to **$249.09** and **$214.69**, using the 100+ SparkFun price for the volume case. [MVP BOM](https://docs.google.com/spreadsheets/d/1GG7O95_0aSs89AQl4g6phD0VFDLTcqFAaMB5zflHOEA/edit?gid=2071228825#gid=2071228825) · [Hardware cost model](https://docs.google.com/spreadsheets/d/1Vo3cIDeer3M5wfd5n27pI2ez8do8EjMCnAkfwn7qf3Q/edit?gid=2071228825#gid=2071228825) · [SparkFun pricing](https://www.sparkfun.com/sparkfun-6-degrees-of-freedom-breakout-lsm6dso-qwiic.html)

### 3.8.4 Software BOM

The workbook budgets **$3,495/month** for Memfault Growth and **$10/month** for a proposed AWS backend. Growth lists 1,000 reporting devices, or 250 fully active kits. The prototype connects directly to Memfault, so we need to confirm whether an AWS backend is needed and get a quote for fleet size and OTA. [Software BOM](https://docs.google.com/spreadsheets/d/1Vo3cIDeer3M5wfd5n27pI2ez8do8EjMCnAkfwn7qf3Q/edit?gid=1647301774#gid=1647301774) · [Memfault pricing](https://memfault.com/pricing/)

### 3.8.5 Financial Model

We plan to rent each kit for **$199 per quarter plus $15 shipping**. When kits come back, we inspect, recharge and reflash them and replace worn straps. The U.S. scenario in the workbook models a first-year loss of **$426,827** and a three-year profit of **$606,901** at 3,000 kits, before recurring software costs. For the ten-kit pilot, we will track how many kits are rented again and what they cost to turn around. [Rental scenario](https://docs.google.com/spreadsheets/d/1Vo3cIDeer3M5wfd5n27pI2ez8do8EjMCnAkfwn7qf3Q/edit?gid=1482974775#gid=1482974775)

### Before Submission

Add the two diagrams. Enter the power assumptions in the workbook, reconcile the IMU and enclosure prices, and include recurring cloud costs in the quarterly rental scenario. Check that the teaching team can open the workbook.
