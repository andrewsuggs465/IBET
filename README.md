# Auburn IBET Team 2 — Security Pouch Prototype

A smart anti-theft security pouch prototype for travelers. Combines RFID-based locking, an alarm siren, and real-time status indication in a single Arduino R4 sketch.

## Features

- **RFID lock/unlock** — first tag scanned enrolls as the owner key; subsequent scans toggle the lock
- **Servo latch** — simulates the physical lock mechanism (0° open, 90° locked)
- **RGB status LED** — green (unarmed), red (armed), blue flash (rejected tag), red/blue strobe (alarm)
- **Buzzer alarm** — two-tone siren triggered by button press while armed
- **Auto-disarm** — alarm shuts off automatically after 30 seconds if not disarmed by RFID

## Hardware

| Component | Part |
|-----------|------|
| Microcontroller | Arduino UNO R4 |
| RFID reader | MFRC522 |
| Lock actuator | Servo motor |
| Status indicator | Common-cathode RGB LED |
| Alarm | Passive buzzer |
| Trigger | Momentary push button |

## Wiring

| Component | Arduino Pin |
|-----------|-------------|
| RFID SS | 10 |
| RFID RST | 5 |
| RFID MOSI/MISO/SCK | 11 / 12 / 13 |
| Servo signal | 9 |
| RGB LED R | 6 |
| RGB LED G | 7 |
| RGB LED B | 8 |
| Buzzer | 3 |
| Button | 2 → GND |

Add 220 Ω resistors in series with each LED leg. The button uses the internal pull-up (no external resistor needed).

## Dependencies

Install via **Arduino IDE → Sketch → Include Library → Manage Libraries**:

| Library | Purpose |
|---------|---------|
| `MFRC522` | RFID reader driver |
| `Servo` | Servo control (usually pre-installed with R4 board package) |

## Usage

1. Upload `starter_code/starter_code.ino` to the Arduino R4.
2. Open the Serial Monitor at **9600 baud**.
3. **Scan your RFID tag** — the tag is enrolled and the bag locks (red LED).
4. **Press the button** — alarm triggers (siren + strobe).
5. **Scan the tag again** — alarm disarms and bag unlocks (green LED).
6. Scanning an unregistered tag flashes blue and is rejected.

## Product Overview

The IBET Security Pouch is a comprehensive travel security solution targeting safety-conscious travelers, tourists, and everyday city users. Key product pillars:

- **Tracking** — cellular (NB-IoT / LTE-M) or BLE (Apple Find My / Google Find Hub) with GNSS
- **Deterrence** — 130 dB siren + strobe alarm
- **Physical security** — Kevlar/composite slash-proof construction, electromagnetic/solenoid latch
- **Remote control** — web dashboard for real-time tracking, remote lock/unlock, arm/disarm
- **Biometric unlock** — fingerprint with RFID backup

This prototype demonstrates the core lock, alarm, and status-indication subsystems.
