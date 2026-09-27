# EVA — ESP32-Based Interactive Robotic Head

</p>

> An embedded systems project combining real-time sensing, signal filtering, event-driven behaviour, servo control, OLED graphics, and PC-side radar visualization using an ESP32 with a cooperative non-blocking scheduler.

<p align="center">
  <img src="doc/images/Normal mode.jpeg" width="400" alt="EVA Interactive Robotic Head">
</p>

---

## Table of Contents

- [Overview](#overview)
- [Demo](#demo)
- [Features](#features)
- [System Architecture](#system-architecture)
- [Hardware](#hardware)
- [Circuit Design](#circuit-design)
- [Firmware Architecture](#firmware-architecture)
- [Core Algorithms](#core-algorithms)
- [Operating Modes](#operating-modes)
- [Radar Visualization](#radar-visualization)
- [Wi-Fi Control Prototype](#wi-fi-control-prototype)
- [Testing and Validation](#testing-and-validation)
- [Repository Structure](#repository-structure)
- [Getting Started](#getting-started)
- [Skills Demonstrated](#skills-demonstrated)
- [References](#references)

---

## Overview

EVA is an ESP32-based interactive robotic head that combines:

**Sensing → Processing → Decision → Response → Visualization**

### Main hardware

- **ESP32-WROOM-32** — main microcontroller
- **HC-SR04** — ultrasonic distance sensing
- **TTP223** — capacitive touch interaction
- **SSD1306 OLED** — expressions and system status
- **SG90 servo** — head movement and scanning
- **5 LEDs** — distance indication

A separate **Processing** application provides real-time radar visualization from serial telemetry.

The firmware uses a **finite-state machine (FSM)**, **event-driven architecture**, **interrupt-based sensing**, and a **cooperative non-blocking scheduler**.

---

# Demo

## Operating Modes

<table>
<tr>
<th align="center">Normal Mode</th>
<th align="center">Observation Mode</th>
</tr>

<tr>
<td align="center">
<img src="doc/images/Normal mode.jpeg" width="280" alt="EVA Normal Mode">
</td>

<td align="center">
<img src="doc/images/Observation.jpeg" width="280" alt="EVA Observation Mode">
</td>
</tr>
</table>

---

## Radar Visualization

<p align="center">
  <img src="doc/images/Radar.jpeg" width="550" alt="EVA radar visualization">
</p>

---

## Wi-Fi Control Prototype

<p align="center">
  <img src="doc/images/wifi ready.jpeg" width="500" alt="EVA Wi-Fi control interface">
</p>

---

# Features

- Interrupt-driven HC-SR04 echo timing
- 5-sample median filtering
- Valid-distance rejection from 3–200 cm
- Confirmation-based person detection
- Cooperative non-blocking task scheduler
- Event-driven architecture using a ring buffer
- Finite-state machine for behavioural control
- Animated OLED expressions
- Servo-based environmental scanning
- Distance indication using five LEDs
- Serial radar telemetry
- Real-time Processing radar visualization
- Standalone ESP32 Wi-Fi control prototype

---

## System architecture

```mermaid
flowchart LR
    subgraph Sensing
        A[HC-SR04 Ultrasonic]
        B[TTP223 Touch]
    end
    subgraph Processing["ESP32 — Processing"]
        C[Median Filter + Validation]
        D[Person Detection Logic]
        E[Finite-State Machine]
        F[Cooperative Scheduler]
    end
    subgraph Response
        G[SG90 Servo]
        H[SSD1306 OLED]
        I[5x Distance LEDs]
    end
    subgraph Visualization
        J[Serial Link]
        K[Processing Radar App]
    end

    A --> C --> D --> E
    B --> E
    F -.orchestrates.-> C
    F -.orchestrates.-> G
    F -.orchestrates.-> H
    E --> G
    E --> H
    C --> I
    E --> J --> K
```

## Hardware

### Bill of materials

| Component | Interface | ESP32 Pin |
|---|---|---|
| SSD1306 OLED (128×64, I²C, addr `0x3C`) | I²C | SDA 21 • SCL 22 |
| HC-SR04 Ultrasonic Sensor | Digital | TRIG 5 • ECHO 23 (via voltage divider) |
| TTP223 Touch Sensor | Digital | GPIO 4 |
| SG90 Servo Motor | PWM @ 50 Hz | GPIO 19 |
| 5× Distance LEDs (220 Ω series) | Digital | 13, 12, 14, 26, 25 |

### Power architecture

| Rail | Used by |
|---|---|
| 3.3 V | SSD1306 OLED, TTP223 touch sensor |
| 5 V / VIN | HC-SR04, SG90 servo |
| GND | Common ground across all modules |

### HC-SR04 echo voltage divider

The HC-SR04 echoes at 5 V; the ESP32 GPIO is 3.3 V-only, so the echo line is stepped down before GPIO 23:

```
HC-SR04 ECHO
     │
    1 kΩ
     │
     ├────── GPIO 23  (≈3.3 V)
     │
    2 kΩ
     │
    GND
```

<p align="center"><img src="doc/images/circuit_dia.jpeg" width="640" alt="Full circuit diagram"/></p>

## Firmware architecture

Firmware is split into independent modules — sensor acquisition, filtering, touch handling, servo control, OLED display, event processing, radar output, and scheduling — so each piece can be tested and modified in isolation.

**Cooperative scheduler — task periods:**

| Task | Period |
|---|---|
| Touch polling | 20 ms |
| Servo update | 20 ms |
| OLED refresh | 40 ms |
| Sensor read | 60 ms |
| Radar telemetry | 60 ms |
| Serial status | 1 s |

The main loop is a single call to `schedulerUpdate()` — each task only runs once its interval has elapsed, so nothing blocks anything else.

**Finite-state behaviour:**

```mermaid
stateDiagram-v2
    [*] --> SLEEPY
    SLEEPY --> HAPPY: person entered
    HAPPY --> SLEEPY: person left
    HAPPY --> LOVE: touch (short press)
    SLEEPY --> LOVE: touch (short press)
    LOVE --> HAPPY: timeout
    HAPPY --> OBSERVATION: touch (long press)
    SLEEPY --> OBSERVATION: touch (long press)
    OBSERVATION --> SLEEPY: touch (long press)
```

Events (`person entered`, `person left`, `touch pressed`, `mode changed`) are pushed into a fixed-size ring buffer and drained by a single event handler — decoupling detection from reaction.

## Core algorithms

**Distance calculation**

```
d = (t × 0.0343) / 2
```
where `t` is echo pulse duration in µs and `0.0343 cm/µs` is the speed of sound; division by 2 accounts for the round trip.

**Signal validation & filtering**
- Valid range: 3–200 cm; anything outside is rejected
- 5-sample median filter on valid readings (rejects isolated noise spikes)
- 8 consecutive invalid readings → sensor-fault state

**Person detection (Normal Mode)**

```mermaid
flowchart TD
    A[Distance Reading] --> B{Valid?}
    B -- No --> F[Fault Counter++]
    B -- Yes --> C[Median Filter]
    C --> D{≤ 60 cm?}
    D -- Yes --> E[2 confirmations → Person Detected]
    D -- No --> G[3 confirmations + 900ms → Person Left]
```

## Operating modes

**Normal Mode** — interactive expressions and responses
- No person → `SLEEPY` · Person detected → `HAPPY` · Touch → `LOVE`
- Servo range: 30°–150° · Love expression: 84°–96°

**Observation Mode** — environmental scanning + telemetry
- Servo sweep: 0°–180° · Detection range: 20–100 cm
- Records detection episodes and estimates occupancy (a single ultrasonic sensor cannot identify individuals — this is deliberately framed as *episodes*, not identity tracking)

## Radar visualization (Processing)

During Observation Mode, EVA streams serial data as:

```
RADAR,angle,distance,detected
RADAR,90,45.2,1
```

A companion **Processing** sketch parses this stream and renders a live 0°–180°, 0–100 cm radar sweep with per-angle detection history.

<p align="center"><img src="doc/images/Radar.jpeg" width="480" alt="Radar visualization"/></p>

## Bonus: phone Wi-Fi control

A standalone prototype turns the ESP32 into its own Wi-Fi access point with a lightweight web UI — no app or internet required:

- Connect to the ESP32 AP → browse to `192.168.4.1`
- Command buttons: `HELLO` `HAPPY` `SLEEP` `LEFT` `CENTER` `RIGHT` `STATUS` `STOP`
- OLED echoes live state back: Wi-Fi status, servo angle, last command, event count

> Kept as a **separate sketch** from the main firmware — it's an exploration of remote/IoT control, not a dependency of the core behavioural system.

## Repository structure

```
Interactive-Robo-Head-/
├── README.md
├── src/
│   ├── EVA_2.0.ino
│   └── Phone_wifi.ino
├── processing_radar/
│   └── EVA_RADAR.pde
└── doc/
    ├── ES_Project_Report.pdf
    ├── EVA_ppt.pdf
    └── images/
        ├── Circuit.jpeg
        ├── Normal mode.jpeg
        ├── Observation.jpeg
        ├── Radar.jpeg
        ├── circuit_dia.jpeg
        ├── control_web.jpeg
        └── wifi ready.jpeg
```

## Getting started

**Requirements**
- Arduino IDE (or PlatformIO) with ESP32 board support
- Libraries: `Adafruit_GFX`, `Adafruit_SSD1306`, `ESP32Servo`
- [Processing](https://processing.org/) 4.x (for the radar visualizer)

**Flash the firmware**
```bash
# Arduino IDE
1. Open firmware/eva_main/eva_main.ino
2. Select board: ESP32 Dev Module
3. Wire hardware per the pin table above
4. Upload
```

**Run the radar visualizer**
```bash
1. Open processing_radar/eva_radar_visualizer.pde in Processing
2. Set the correct serial port
3. Run — put EVA into Observation Mode (long touch press)
```

## Testing & validation

- **Hardware tests:** ESP32, OLED, HC-SR04, TTP223, servo, LEDs — verified individually
- **Integration tests:** distance + LED indication, person detection, touch interaction, servo scanning, OLED expressions, Observation Mode, radar communication
- **Functional validation:** distance-based interaction, touch input, servo movement, OLED expressions, observation scanning, and PC-based radar visualization all confirmed working end-to-end on hardware

## Skills demonstrated

`Embedded C/C++` · `Interrupt-driven I/O` · `Digital signal filtering` · `Finite-state machine design` · `Event-driven architecture` · `Cooperative real-time scheduling` · `I²C / PWM / GPIO interfacing` · `Voltage-divider circuit design` · `Serial protocol design` · `PC-side visualization (Processing/Java)` · `Wi-Fi AP + HTTP control` · `Hardware bring-up & incremental testing`


## References

1. ESP32-WROOM-32 Development Board documentation
2. HC-SR04 Ultrasonic Sensor documentation
3. SSD1306 OLED controller documentation
4. TTP223 Touch Sensor documentation
5. SG90 Servo Motor documentation
6. Arduino ESP32 documentation
7. Adafruit GFX Library documentation
8. Adafruit SSD1306 Library documentation
9. ESP32Servo Library documentation
10. Processing IDE 4 documentation

---

<p align="center"><i>Built by Disha Paralkar</i></p>
