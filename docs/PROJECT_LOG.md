# BuddyGuard project log

**ESE 5180 F26 · Team 02 · Jet2Holiday**

This log follows the [IoT Venture manual](https://docs.google.com/document/d/1gKqiMN2WEVwAf0uwNyS8Gh1MBPQqg_3FwDCZpiu_ctY/edit). Evidence links record what the team built or observed. Submission and checkoff status require their own records.

## Course milestones

| Section | Work and evidence | Status in this repository |
| --- | --- | --- |
| 3.1 Team Formation | Team name, number, member names and emails are in the [root README](../README.md). | Roster recorded. |
| 3.2 Concept Development | The [early concept](https://github.com/ese5180/f26-t02-wearable-buddy-system/blob/befa0bd5cff494ed45739741b42ae2e18b51cb8f/README.md) was an AI approval console. The team pivoted to a camp group-supervision prototype. See the [refined BuddyGuard concept](CONCEPT.md) for the current product and market. | Concept and pivot recorded. |
| 3.3 Pitch #1 | Link the final deck and record feedback here when available. | Deck and feedback not verified. |
| 3.4 MVP BOM | [BuddyGuard MVP BOM](https://docs.google.com/spreadsheets/d/1GG7O95_0aSs89AQl4g6phD0VFDLTcqFAaMB5zflHOEA/edit?gid=2071228825#gid=2071228825). The bench setup uses XIAO ESP32S3 and the Wio-SX1262 B2B kit. | BOM linked. See the firmware guides for the parts actually tested. |
| 3.5 Wireless Demo and Secure Firmware Updates | BLE presence and counselor roster, LoRa STATUS/SOS/QUERY/REPLY, optional IMU, MCUboot-signed BLE OTA and optional UART recovery. [Checkoff answers](en/FIRMWARE_SECURITY_CHECKOFF.md) · [Hardware results](VALIDATION.md#earlier-hardware-validation). | Bench-tested. |
| 3.6 Fleet Management | Memfault diagnostics, direct signed Wi-Fi OTA, system and product metrics, reboot reasons, coredumps, and a cohort-targeted A/B rollout. [README](submissions/3.6-fleet-management/README.md) · [PDF](submissions/3.6-fleet-management/README.pdf) · [Bench results](VALIDATION.md#ab-cohort-rollout-2026-11-08). | Bench-tested. Submission files in Git. |
| 3.7 Closing the Loop: HIL Testing + AI | Bench results exist for the radio, update paths and IMU. Record the course HIL workflow, peripheral tests and before/after low-power measurements here when available. | Course HIL deliverable not verified. |
| 3.8 Concept Refinement | The [refined concept](CONCEPT.md) covers market, competitors, stakeholder targets, requirements and an audit of the power and cost model. | Written. Diagrams, battery measurements and cost reconciliation are open. |
| 3.9 Pitch #2 | Link the final pitch deck and project overview slide. | Pitch materials not verified. |
| 3.10 Stakeholder Meeting | Record stakeholder feedback and resulting design changes. | BuddyGuard meeting notes not verified. |
| 3.11 MVP Demo | The [Fleet checkoff card](zh/FLEET_CHECKOFF.md) and [wireless setup guide](en/GETTING_STARTED.md) cover the current two-board prototype. | Component demos documented. Integrated MVP checkoff not verified. |
| 3.12 Demo Day | Rehearse the full product demo and link the final pitch materials. | Demo Day materials not verified. |
| 3.13 Final Report (Website) | Use this log, the firmware guides and the validation record as sources for the final site. Add the published URL, final video, hardware photos and retrospective. | Website and final report not verified. |

## Engineering sequence

1. **Board bring-up.** Provisioned the XIAO boards and got BLE presence and signed BLE OTA working. The first [bench checkpoint](https://github.com/ese5180/f26-t02-wearable-buddy-system/blob/6934668cc72a7f560da7fe432c1bddaa06ef2a06/README.md) records the unresolved LoRa SPI connection.
2. **Local wireless.** Seated the Wio B2B link and verified the SX1262 sync word `0x1424`. Added LoRa status, query/reply, SOS, ACK and bounded retries alongside the BLE roster. Added IMU motion as an optional snapshot. Confirmed the missing-tag, LoRa query and BLE recovery sequence on two boards.
3. **Signed updates.** Staged BLE SMP downloads in secondary flash. Tested valid installation, interrupted upload, wrong-signature and corrupted-image rejection, and watchdog rollback of an unconfirmed image. UART recovery was separately tested on C01.
4. **Fleet diagnostics and Wi-Fi OTA.** Added the Memfault SDK, runtime Wi-Fi provisioning and signed HTTPS downloads. Verified cloud diagnostics, release targeting and the A/B report cadence in the team bench session. [Fleet implementation source](https://github.com/Alex-lei-1/buddyguard/commit/5a730b1823df74a9970945415dc7b4f58a03e83f).
5. **Integration and handoff.** The course repository combines the wireless, recovery and Fleet work. The [source map](../firmware/README.md) lists the module boundaries. The [documentation index](README.md) points to both languages and the submission copy.

## Prototype limits

The bench record covers one tag and one counselor. Longer runs, outdoor range, full multi-tag behavior and battery measurements still need evidence. Device reports observe USB host activity. Application signatures are verified by MCUboot. Hardware secure-boot eFuses, flash encryption, authenticated management access and authenticated LoRa messages remain future work.
