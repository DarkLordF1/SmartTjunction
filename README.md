# SmartTjunction

A cooperative multitasking simulation and control system for a smart T-junction traffic light network, implemented on a **Raspberry Pi Pico** and an **ILI9341 TFT display**. This project was developed as part of the *Real-Time Systems Laboratory* course (0512-4494) at **Tel Aviv University**.

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Wiring](#wiring)
- [Software Requirements](#software-requirements)
- [Installation](#installation)
- [Usage](#usage)
- [Repository Structure](#repository-structure)
- [License & Credits](#license--credits)

---

## Overview

SmartTjunction emulates a real-world T-junction traffic control system by combining time-based logic, vehicle flow simulation, and live feedback on a graphical dashboard. It demonstrates:

1. **Vehicle generation** using:
   - Manual mode (pushbuttons)
   - Stochastic Markov-like random generation

2. **Traffic light sequencing** using round-robin scheduling:
   - Red → Yellow (0.5s) → Green (1–7s) per light

3. **Dynamic optimization**:
   - Every 30 seconds, the system re-evaluates queue lengths and thrown vehicles to adjust green-light durations dynamically.

4. **Live monitoring**:
   - A graphical UI on the TFT shows light states, mode, vehicle counts, and optimizations.

5. **Serial logging**:
   - Logs key events with timestamps (light changes, vehicle flow, optimizations) over USB serial at 57600 baud.

---

## Features

- 🚦 **3-Lane Traffic Light System** with accurate light transitions
- 🤖 **Dual Vehicle Generation Modes** (manual/random)
- 📊 **Real-Time Optimization** based on traffic flow
- 🖥️ **TFT Dashboard UI** with icon-based updates
- 📤 **Serial Log Output** for debugging and analysis
- 🧠 **Modular FSM Architecture** for traffic control logic

---

## Hardware Requirements

- Raspberry Pi Pico (or compatible RP2040 board)
- ILI9341 2.8" TFT Display (SPI interface)
- 3× Pushbuttons (for vehicle injection)
- 1× Slide switch (for mode selection)
- RGB LEDs (or simulated display)
- Breadboard, jumper wires, resistors

---

## Wiring

| Component        | Pico Pin | Description                             |
|------------------|----------|-----------------------------------------|
| TL1 Red LED      | GP10     | Lane 1 red light                        |
| TL1 Green LED    | GP9      | Lane 1 green light                      |
| TL2 Red LED      | GP7      | Lane 2 red light                        |
| TL2 Green LED    | GP6      | Lane 2 green light                      |
| TL3 Red LED      | GP4      | Lane 3 red light                        |
| TL3 Green LED    | GP3      | Lane 3 green light                      |
| Button 1         | GP14     | Inject vehicle into lane 1              |
| Button 2         | GP13     | Inject vehicle into lane 2              |
| Button 3         | GP12     | Inject vehicle into lane 3              |
| Mode Switch      | GP11     | Slide switch: HIGH = manual, LOW = auto |
| TFT CS           | GP22     | TFT chip select                         |
| TFT DC           | GP20     | TFT data/command                        |
| TFT RST          | GP21     | TFT reset                               |
| TFT MOSI         | GP19     | SPI MOSI                                |
| TFT SCK          | GP18     | SPI clock                               |
| TFT MISO         | GP16     | SPI MISO                                |

---

## Software Requirements

- Arduino IDE or VS Code with PlatformIO
- Board support for **Raspberry Pi Pico**
- Required libraries:
  - `Adafruit GFX`
  - `Adafruit ILI9341`

---

## Installation

```bash
git clone https://github.com/aiman5abed/SmartTjunction.git
cd SmartTjunction
```
Open traffic_control.ino in your IDE.
Install the required libraries listed in libraries.txt.
Connect your Pico and select the correct board/port.
Upload the sketch.
Usage

Wire the components as shown above.
Power up the board and open Serial Monitor at 57600 baud.
Use the slide switch to toggle between manual and random vehicle generation.
Observe real-time updates on the TFT screen:
Light status
Mode
Queue size
Optimizer reports
Logs will appear in Serial Monitor with timestamps.
Example log output:

12345 ms, system, TL1 → GREEN
12401 ms, vehicle 5, generated, L1:1 L2:0 L3:0
13245 ms, vehicle 5, crossed from lane 1
30345 ms, system, optimizer adjusted TL2 to 6s green
Repository Structure
```
SmartTjunction/
├── traffic_control.ino             # Main code
├── diagram.json                    # Wokwi circuit layout
├── libraries.txt                   # Required libraries
├── presentation_Smart-T-Junction.pptx  # Final slides
└── README.md
```
## License & Credits

This project is licensed under the Tel Aviv University Educational License (TAU-EL).
See LICENSE for full terms.

Developed as part of course 0512-4494 – Real-Time Systems Lab.

Authors:

Aiman Abed – GitHub
Edward Khoury – Co-developer
<p align="center"> <img src="https://en-engineering.tau.ac.il/sites/engineering-english.tau.ac.il/files/TAU_facultot_logos-01-handasa_0.png" alt="Tel Aviv University" height="72" width="212"> </p> 
