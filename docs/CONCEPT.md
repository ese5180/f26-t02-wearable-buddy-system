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

The Greater Philadelphia YMCA's camp handbook prohibits camper cell phones and describes lost-camper drills and injury response. Camps already have a procedure for what to do when someone is missing or hurt. We're looking at the gap before staff notice that something happened. [Greater Philadelphia YMCA 2026 handbook, pp. 13, 18–19](https://www.philaymca.org/sites/default/files/2026-01/2026%20Camp%20Handbook%20Final.pdf)

#### Product Name & Function

BuddyGuard gives campers screen-free tags that check in over BLE and gives counselors a handheld that flags missed check-ins. The tag is designed to send LoRa SOS and possible-fall alerts, share an available GPS fix during an incident, and vibrate when the counselor sends a prompt. Field supervision runs on BLE and LoRa without Wi-Fi or cellular service, while Wi-Fi at base handles diagnostics and signed updates.

The October proposal made group monitoring the default, with buddy pairs as an optional mode for trips. We're validating group monitoring first. Both roles now use the same XIAO ESP32S3 / Wio-SX1262 kit. We tested BLE, LoRa, IMU movement and cloud updates with one tag and one counselor. GPS and vibration are in the MVP BOM and still need integration. Fall detection still needs validation. [Bench results](VALIDATION.md)

#### Target Market & Demographics

Campers wear the tags. Counselors carry the receiver. The buyer is the camp operator, typically a camp director or recreation department. We're starting with Philadelphia day camps serving children ages 6–12, particularly programs that take field trips or move groups between sites.

The American Camp Association counts about **20,000 U.S. camps** across day and overnight programs. Philadelphia Parks & Recreation runs **more than 100 neighborhood day camps** and serves **more than 7,000 campers** across its summer camps. [ACA camp data](https://www.acacamps.org/sites/default/files/2025-07/ACA-Camp-Stabilization-Toolkit-2025.pdf) · [Philadelphia Parks & Recreation](https://www.phila.gov/services/culture-recreation/activities-and-events/register-for-summer-camp/)

For a full group, we use **one counselor unit and eight tags** as a planning baseline, with six and ten tags as low and high cases. The YMCA publishes staff-to-camper ratios of 1:6, 1:8 and 1:10 by age. Those ratios inform the estimate. Actual group size and the number of groups a site would equip still need to be confirmed. We assume **two equipped groups per site** and one summer rental quarter per year. [Greater Philadelphia YMCA staffing ratios](https://www.philaymca.org/camps/summer-day-camp/)

| Market | Sizing assumption | Equipment or rental estimate |
| --- | --- | --- |
| **TAM** · U.S. camps, upper bound | 20,000 camps × 2 groups | **40,000 counselor units + 320,000 tags** at eight tags per group |
| **SAM** · Philadelphia neighborhood day camps | 100 sites × 2 groups | **200 counselor units + 1,600 tags** at eight tags per group |
| **SOM** · First summer paid pilot | 2 sites × 2–3 pilot kits × $199 | **$796–$1,194** in rental revenue before shipping |

At six or ten tags per group, the Philadelphia estimate is **1,200 or 2,000 tags**. The **$199 per quarter** quote covers the three-tag pilot kit. At that price and two pilot kits per site, the rental benchmarks are **$7.96 million/year nationally** and **$39,800/year in Philadelphia**. The eight-tag configuration needs its own price and a capacity test before we forecast revenue for it. [Rental model](https://docs.google.com/spreadsheets/d/1Vo3cIDeer3M5wfd5n27pI2ez8do8EjMCnAkfwn7qf3Q/edit?gid=1482974775#gid=1482974775)

#### Competitors and current practice

- **Headcounts and buddy checks** are already part of camp trips. They give staff a clear way to account for children at checkpoints. BuddyGuard adds an alert between checks, and the counselor follows the camp’s existing response procedure. [Example camp trip safety plan](https://www.acacamps.org/sites/default/files/resource_library/07-PA-NewYorkStateGuidelinesforplanning.pdf#page=33)
- **Campminder / Campanion** helps staff record attendance and manage bus rosters. BuddyGuard can flag a missed check-in between headcounts, while the camp keeps its attendance system. [Campminder transportation](https://campminder.com/features/transportation/)
- **Garmin Bounce 2** gives families live location and two-way calling through an LTE service. Garmin announced it at $299.99 per watch and $9.99 per month for LTE. BuddyGuard gives camp staff one receiver for the group, with screen-free tags and local alerts that need no phone or cellular plan. [Garmin announcement](https://www.garmin.com/en-US/newsroom/press-release/sports-fitness/parents-can-delay-the-smartphone-with-bounce-2-kids-smartwatch-from-garmin/)
- **Nelos** already offers GPS tracking and group messaging over LoRa without cell service, using a phone paired with a radio. BuddyGuard uses a dedicated counselor unit for missing-tag alerts and plans to add possible-fall alerts. Staff can request a location if they need it. [Nelos](https://nelos.app/)

#### Stakeholders

We want to speak with a director and a lead counselor at a [Greater Philadelphia YMCA day camp](https://www.philaymca.org/camps/summer-day-camp/) or a [Philadelphia Parks & Recreation site](https://www.phila.gov/services/culture-recreation/activities-and-events/register-for-summer-camp/). We also want to talk to a [Summer Achievers host-site operator](https://www.phila.gov/2026-05-08-register-now-make-this-summer-unforgettable-with-summer-achievers-summer-camp-at-edey-schools-2/) who organizes trips and summer activities. We want to find out how they handle a missing or injured camper, which trips have poor network coverage, and who approves a rental. A director and a counselor can also tell us what they need to see in an alert and how many children are in a group and how many groups they would equip. These interviews are still pending.

#### System-Level Diagrams

**Hardware**

```mermaid
flowchart TB
    subgraph HARDWARE["BuddyGuard · Hardware"]
        direction LR

        subgraph SENSORS["Sensors · Student Tag"]
            direction TB
            IMU["IMU · LSM6DSO"]
            GNSS["GNSS · L76K"]
            SOS["SOS Button"]
            CLASP["Magnetic Clasp Sensor"]
        end

        MCU["XIAO ESP32S3"]

        subgraph ACTUATORS["Actuators"]
            direction TB
            LCD["LCD<br/>Counselor Handheld"]
            MOTOR["Vibration Motor<br/>Student Tag"]
        end

        IMU <-->|"I2C 3.3 V"| MCU
        GNSS -->|"UART 3.3 V"| MCU
        SOS -->|"GPIO 3.3 V"| MCU
        CLASP -->|"GPIO 3.3 V"| MCU
        MCU -->|"SPI 3.3 V"| LCD
        MCU -->|"GPIO 3.3 V"| MOTOR
    end

    subgraph POWER["Power"]
        direction LR
        LIPO["LiPo"] --> REG["Regulator"] --> RAIL["3.3 V Rail"]
    end

    HARDWARE ~~~ POWER

    style HARDWARE fill:#F8FAFC,stroke:#CBD5E1
    style SENSORS fill:#ECFDF5,stroke:#74B9A3,color:#145344
    style MCU fill:#203A63,stroke:#142746,color:#FFFFFF
    style ACTUATORS fill:#FFF3E5,stroke:#D9A365,color:#653B15
    style POWER fill:#FFF8DC,stroke:#CBB567,color:#5C4814
```

Both units use the XIAO ESP32S3. The student tag carries the IMU, GNSS, SOS button, clasp sensor and vibration motor. The counselor handheld uses an LCD to show local alerts. The motor's GPIO signal controls a MOSFET driver.

**Communication**

```mermaid
flowchart LR
    TAG["Student Tag"]
    COUNSELOR["Counselor Handheld<br/>Local alerts on LCD"]
    CLOUD["Memfault Cloud<br/>Device health and update status"]

    TAG -->|"BLE advertising · 2.4 GHz<br/>Presence heartbeat"| COUNSELOR
    TAG <-->|"LoRa P2P · 915 MHz<br/>Status, SOS, reply →<br/>← Query, ACK"| COUNSELOR
    COUNSELOR <-->|"Camp Wi-Fi · HTTPS · at base<br/>Diagnostics →<br/>← Signed firmware"| CLOUD

    style TAG fill:#ECFDF5,stroke:#74B9A3,color:#145344
    style COUNSELOR fill:#203A63,stroke:#142746,color:#FFFFFF
    style CLOUD fill:#FFF3E5,stroke:#D9A365,color:#653B15
```

In the field, tags broadcast BLE check-ins and exchange LoRa packets directly with the assigned counselor handheld. The handheld flags missed check-ins on its LCD. Field supervision uses a star topology with the handheld at the center and runs without Wi-Fi or cellular service. At base, the handheld joins camp Wi-Fi, uploads device diagnostics over HTTPS, and pulls signed firmware from Memfault. The Memfault dashboard shows fleet health and update status. [Firmware guide](en/FIRMWARE.md)

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
- **HRS 04.** The screen-free wrist tag shall have a physical SOS button and a driver-controlled vibration motor. Check fit, send SOS and run the motor without resetting the tag.
- **HRS 05.** The counselor unit shall display the affected tag ID and alert type. Trigger a missing-tag alert and an SOS to check the display.

#### Software Requirements Specification

**Overview.** Zephyr runs the tag and counselor firmware. The counselor tracks presence and handles local incident alerts. The Wi-Fi worker handles cloud reporting and updates at base.

**Users.** Camp counselors use alerts and location requests. Fleet administrators manage devices and firmware.

**Definitions.** A heartbeat is a BLE presence message. A GPS fix is a position with a recorded time. A possible-fall alert is a prompt for staff to check on a camper.

**Functionality**

- **SRS 01.** With Wi-Fi and cellular service unavailable, the counselor shall monitor three tags and flag a previously seen tag within 11 seconds of its last fresh BLE message in the bench profile. Run a 30-minute group test, stop one tag and record time to alert and false alerts.
- **SRS 02.** The counselor shall query a missing tag over LoRa, stop after at most three attempts, and show reply or no reply. A fresh BLE message shall clear the missing state. Test reachable and unreachable tags.
- **SRS 03.** The tag shall send a possible-fall alert when the IMU classifier flags an event. Compare test-dummy falls with walking, running, jumping and removing the tag. Record misses and false alarms.
- **SRS 04.** The counselor shall be able to request a GPS fix and its age during an incident. A missing or stale fix shall be identified, and an SOS shall go out without waiting for a fix. Test with and without satellite reception.
- **SRS 05.** At base, devices shall report diagnostics and running firmware version to Memfault and stage signed updates for a trial boot. Verify a successful update and a rollback after watchdog reset before confirmation.

The bench demo has been verified with one tag and one counselor unit. The three-tag pilot test is still open. The commercial sizing baseline is eight tags per counselor unit, with six and ten as sensitivity cases. That capacity still needs testing. The fall classifier and GPS incident reporting still need testing. [Bench results](VALIDATION.md)

### 3.8.2 Power Budget

The BOM uses a 400 mAh LiPo. With 80% usable capacity, the tag has about 320 mAh. The first-pass estimates below use vendor currents as approximate battery-side loads, with Wi-Fi off.

| Tag mode | Approximate draw | Estimated runtime |
| --- | ---: | ---: |
| BLE active, LoRa receiving, IMU on, GPS in standby | 90 mA | 3.6 h |
| Same conditions with GPS acquiring or tracking continuously | 131 mA | 2.4 h |

The L76K adds about 41 mA when active. The motor draws about 60 mA at 3 V while vibrating. Neither estimate includes LoRa transmit bursts or vibration duty cycle. Our target is 10 hours on a charge, which requires **32 mA or less** on this battery. Verify it with a battery discharge test and measure GPS standby, radio bursts and vibration on the assembled tag. [XIAO](https://wiki.seeedstudio.com/xiao_esp32s3_getting_started/) · [SX1262](https://www.semtech.com/products/wireless-rf/lora-connect/sx1262) · [LSM6DSO](https://www.st.com/en/mems-and-sensors/lsm6dso.html) · [L76K](https://wiki.seeedstudio.com/get_start_l76k_gnss/) · [Vibration motor](https://www.adafruit.com/product/1201)

### 3.8.3 Hardware BOM

The current BOM prices the three-tag pilot kit. Each tag has GPS, an IMU and a driver-controlled vibration motor. The kit includes one counselor unit. The workbook includes batteries, enclosures, straps, fasteners, charging and packaging. It totals **$228.09 for one kit** and **$195.34 per kit at 1,000 kits** using the listed Adafruit IMU. Substituting the bench-tested SparkFun IMU on all three tags would bring those estimates to **$249.09** and **$214.69**, using the 100+ SparkFun price for the volume case. [MVP BOM](https://docs.google.com/spreadsheets/d/1GG7O95_0aSs89AQl4g6phD0VFDLTcqFAaMB5zflHOEA/edit?gid=2071228825#gid=2071228825) · [Hardware cost model](https://docs.google.com/spreadsheets/d/1Vo3cIDeer3M5wfd5n27pI2ez8do8EjMCnAkfwn7qf3Q/edit?gid=2071228825#gid=2071228825) · [SparkFun pricing](https://www.sparkfun.com/sparkfun-6-degrees-of-freedom-breakout-lsm6dso-qwiic.html)

Adding five complete tags at the same BOM rates gives an **eight-tag material estimate of about $510–$577 per kit**. It uses the SparkFun IMU and carries forward the pilot kit’s charging and packaging estimates. Get quotes for a nine-device charging setup, enclosure processing and assembly before treating this as a finished-kit cost.

### 3.8.4 Software BOM

The workbook budgets **$3,495/month** for Memfault Growth and **$10/month** for a proposed AWS backend. Growth lists 1,000 reporting devices, or 250 fully active four-device pilot kits. The prototype connects directly to Memfault, so we need to confirm whether an AWS backend is needed and get a quote for fleet size and OTA. [Software BOM](https://docs.google.com/spreadsheets/d/1Vo3cIDeer3M5wfd5n27pI2ez8do8EjMCnAkfwn7qf3Q/edit?gid=1647301774#gid=1647301774) · [Memfault pricing](https://memfault.com/pricing/)

### 3.8.5 Financial Model

**Paid pilot.** Offer the three-tag kit at **$199 per summer quarter plus $15 shipping**. Start with four to six kits across two sites. Measure time to notice a staged separation, false alerts, actual turnaround cost and paid renewals. The workbook’s 3,000-kit scenario uses the four-device kit and excludes recurring software costs. [Existing rental worksheet](https://docs.google.com/spreadsheets/d/1Vo3cIDeer3M5wfd5n27pI2ez8do8EjMCnAkfwn7qf3Q/edit?gid=1482974775#gid=1482974775)

#### Eight-tag investor scenario

For an eight-tag kit and one counselor unit, use **$449 per summer quarter** as an interview price. Assume **$650** upfront for a finished kit, **$50** per paid rental for turnaround and **$40** per owned kit per year for cloud service. These are planning inputs. The $650 figure is a cost target, and $40 requires a commercial cloud quote or a different reporting setup.

| Share of summer quarters rented | Average paid rentals over 3 years | Three-year result per kit |
| --- | ---: | ---: |
| 50% | 1.50 | **−$172** |
| 75% | 2.25 | **+$128 (13% of rental revenue)** |
| 90% | 2.70 | **+$307 (25% of rental revenue)** |

One summer quarter is available each year. The calculation is paid rentals × ($449 − $50) − $650 − (3 × $40). It includes the kit, turnaround and cloud. Company R&D, sales and overhead are outside this calculation. Outbound shipping is billed separately and assumed to cover its cost.

Cloud is the largest unverified cost. If all nine devices report, the listed Memfault Growth plan covers 111 full kits and costs roughly **$378 per kit per year** at full allocation on a twelve-month subscription. Confirm seasonal pricing, OTA coverage and whether all tags need to report before expanding the fleet. Keep larger kit orders tied to bookings and measured renewal and repair costs. [Memfault pricing](https://memfault.com/pricing/)

### Before Submission

Record the camp interviews for the stakeholder section. Enter the power assumptions in the workbook, reconcile the IMU and enclosure costs, and include recurring cloud costs in the rental model. Validate group size and the eight-tag price with camp operators. Check that the teaching team can open the workbook. [Course manual, sections 3.2 and 3.8 and the requirements appendix](https://docs.google.com/document/d/1gKqiMN2WEVwAf0uwNyS8Gh1MBPQqg_3FwDCZpiu_ctY/edit)
