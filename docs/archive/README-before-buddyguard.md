# ESE5180: IoT Venture

**Team Number: 02**

**Team Name: Jet2Holiday**

| Team Member Name | Email Address                  |
| ---------------- | ------------------------------ |
| Siwei Lei        | laialex@engineering.upenn.edu  |
| Rico Zhuang      | zzhuan13@engineering.upenn.edu |
| Yunzhe Deng      | deng1@engineering.upenn.edu    |
| Yubin Guan       | guan1@engineering.upenn.edu    |

**GitHub Repository URL: [https://github.com/yunzhedeng/ese5180-IoT-Venture.git](https://github.com/yunzhedeng/ese5180-IoT-Venture.git)**

## Concept Development

### Product Function

We are building a simple physical console for monitoring and approving AI agent actions. Unlike traditional macro pads, our device connects directly to AI and cloud services. Users can see agent status, review tasks, and approve sensitive actions with one press.

### Target Market & Demographics

**Who will be using your product?**

Our intended users are students, software developers, and productivity enthusiasts who want an affordable, compact keyboard for shortcuts and AI-assisted workflows. We would particularly target privacy-conscious users who value control over their device’s firmware, settings, and data handling.

**Who will be purchasing your product?**

Individual users would purchase the keyboard for their own workstations, while small businesses and engineering teams could buy it for employees. University laboratories and student organizations would also be potential buyers for affordable, customizable input devices.

**Where would you deploy your product?**

We would initially launch in the United States, focusing on university campuses, home offices, and small-business workstations. After validating demand and reliability, we would expand internationally through online sales.

**How large is the market you’re targeting, in US dollars?**

The United States had approximately 1.7 million software developer jobs in 2025, providing a starting point for estimating our initial customer base. Assuming 10% of that population would consider a dedicated macropad at a proposed &#36;40 retail price, our initial target market would represent approximately &#36;6.8 million in hardware purchases. This is an assumption-based, one-device-per-customer estimate—not measured demand or annual market revenue.

**How much of that market do you expect to capture, in US dollars?**

Our first-year sales target would be 1,000 units at &#36;40 each, generating &#36;40,000 in revenue, or approximately 0.6% of our estimated target market. We would test this target through preorders, campus demonstrations, and small-business pilot sales before scaling production.

**What competitors are already in the space?**

Our closest competitor is OpenAI and Work Louder’s Codex Micro, listed at &#36;230. Other alternatives include Elgato’s Stream Deck Neo, listed at &#36;99.99 before promotions, and Adafruit’s MacroPad RP2040 kit. Our intended advantage would be a lower-cost, ready-to-use product with local configuration and clearly documented security controls.

### Stakeholders

- Zapier: for workflow automation
- Anthropic: for Claude/Claude Code power users
- Browser Use: for browser agents and AI automation
- YaoEdge: for AI agent monitoring and workflow control

The keypad gives their users a physical, customizable interface for launching AI agents and repetitive workflows with one press.

We have already spoken with the founder of YaoEdge about our product. He said he is very interested in what we are building and would be willing to invest in the company.

### System-Level Diagrams

![System-Level Diagram](image/README/image.png)

### Security Requirements Specification

- **SecRS-01** - The console shall exchange task and approval data only with an authenticated computer companion application over an encrypted BLE connection.
- **SecRS-02** - The nRF5340 shall verify firmware signatures before execution and reject unauthorized firmware.
- **SecRS-03** - Sensitive actions shall require an external keypad press after the action is displayed. The companion application shall reject expired or reused approvals.

### Hardware Requirements Specification

- **HRS-01** — Wi-Fi Connectivity: The console shall communicate directly with the backend through an nRF7002 Wi-Fi interface without a computer gateway, at a distance of 5 m from the access point in the same room with no obstructions.
- **HRS-02** — Display: The console shall include at least one display capable of presenting the selected agent’s identity, requested action, target resource, and risk level. Scrolling or paging may be used to show complete information.
- **HRS-03** — Physical Controls: The console shall include two separate buttons for approval and rejection and one rotary encoder for selection. The approval button shall support continuous press detection for at least  2 seconds.
- **HRS-04** — Protected Key Storage: The console shall use an nRF5340 and provide hardware-backed isolation for its approval-signing private key. Non-secure application code shall not be able to read or export the

### Software Requirements Specification

- **SRS-01** — Agent Monitoring: The software shall simultaneously track at least  two agent instances, supporting Working, Approval Required, Completed, and Failed states. Under normal connectivity on the same local network, the display shall update within 2 seconds of a backend status event.
- **SRS-02** — Physical Approval: Approval shall require a continuous button press of at least  2 seconds **. Releasing the button early or changing the selected request shall cancel confirmation. Local feedback shall appear within 500 ms after approval is completed or rejection is pressed.
- **SRS-03** — Authorization Enforcement: Each approval shall be signed and bound to the request ID, agent identity, action, target resource, and expiration time. The backend shall permit execution only after verification, allow each authorization to be used  once , and reject invalid, mismatched, expired, or reused authorizations.
- **SRS-04** — Audit Logging: The backend shall persist each received approval or rejection response before acknowledging completion. Each record shall contain the request ID, agent identity, action, target resource, decision, device ID, server timestamp, and validation result, and remain retrievable after a backend restart.
