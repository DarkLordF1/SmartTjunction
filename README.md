# SmartTjunction

A cooperative‐multitasking simulation and control system for a T-junction traffic light network, implemented on a Raspberry Pi Pico (or compatible Arduino board) and an ILI9341 TFT display.

## Table of Contents

* [Overview](#overview)
* [Features](#features)
* [Hardware Requirements](#hardware-requirements)
* [Wiring](#wiring)
* [Software Requirements](#software-requirements)
* [Installation & Usage](#installation--usage)
* [Repository Structure](#repository-structure)
* [License](#license)

## Overview

SmartTjunction simulates a “smart” traffic‐control room for a T-junction with three traffic lights and three lanes. The system:

1. **Generates vehicles** in two modes:

   * **Manual** (via pushbuttons)
   * **Random** (per‐lane Markov process)
2. **Schedules traffic lights** (red/yellow/green) using round‐robin, ensuring each light turns green once per cycle .
3. **Crosses one vehicle per second** when a lane has green.
4. **Periodically optimizes** green‐time durations every 30 s based on “thrown” vehicles to reduce congestion .
5. **Displays live status** on an ILI9341 TFT (mode, light colors, queue lengths, pattern, thrown count).
6. **Logs events** (light changes, vehicle generation, crossings, optimizations) over serial with timestamps .

## Features

* **Traffic Lights (TL1, TL2, TL3)** with mutually exclusive red, yellow (0.5 s), and green (1–7 s) states .
* **Two Vehicle‐Generation Modes**

  * *Manual*: Buttons mapped to lanes
  * *Random*: Markov‐chain with 5 s constant or 0.5–5 s uniform intervals .
* **Queue Management**: Up to 4 cars per lane; excess are “thrown” and logged.
* **Live LCD Dashboard**:

  ```
  MODE: Manual/Random
  TL#1: RED/GREEN/YELLOW
  TL#2: …
  TL#3: …
  CARS: L1:0 L2:2 L3:1
  GREEN: 3,3,3
  THROWN: 1
  ```
* **Graphical Traffic-Light Icons** drawn on screen.
* **Serial Logging** at 57 600 baud:

  ```
  12345ms, system, TL1 → GREEN
  12346ms, vehicle 5, generated, L1:1, L2:0, L3:0
  13345ms, system, crossed from lane 1 (vehicle 5)
  …  
  ```

## Hardware Requirements

* **Raspberry Pi Pico** (or equivalent RP2040-based board)
* **3× RGB LEDs** (one per traffic light)
* **ILI9341 TFT Display**
* **3× Pushbuttons** (vehicle‐generation)
* **1× Slide Switch** (mode selector)
* **Resistors, wires**, breadboard, etc.&#x20;

## Wiring

| Signal      | Board Pin | Device                                 |
| ----------- | --------- | -------------------------------------- |
| TL1 Red     | GP10      | RGB LED 1 Red                          |
| TL1 Green   | GP9       | RGB LED 1 Green                        |
| TL2 Red     | GP7       | RGB LED 2 Red                          |
| TL2 Green   | GP6       | RGB LED 2 Green                        |
| TL3 Red     | GP4       | RGB LED 3 Red                          |
| TL3 Green   | GP3       | RGB LED 3 Green                        |
| Button 1    | GP14      | Pushbutton 1                           |
| Button 2    | GP13      | Pushbutton 2                           |
| Button 3    | GP12      | Pushbutton 3                           |
| Mode Switch | GP11      | Slide Switch (HIGH=Manual, LOW=Random) |
| TFT CS      | GP22      | ILI9341 CS                             |
| TFT DC      | GP20      | ILI9341 DC                             |
| TFT RST     | GP21      | ILI9341 RST                            |
| TFT MOSI    | GP19      | SPI MOSI                               |
| TFT SCK     | GP18      | SPI SCK                                |
| TFT MISO    | GP16      | SPI MISO                               |

*(Adjust pins in `sketch.ino` if you use different wiring.)*

## Software Requirements

* **Arduino IDE** (or VS Code + PlatformIO) with RP2040 support
* **Adafruit GFX** and **Adafruit ILI9341** libraries
* **SPI** library (bundled with Arduino)
* Optional: **Wokwi** for cloud simulation (use `wokwi.toml` provided)

### Installation

1. Clone this repo:

   ```bash
   git clone https://github.com/DarkLordF1/SmartTjunction.git
   cd SmartTjunction
   ```
2. Open `sketch.ino` in Arduino IDE (or your editor of choice).
3. Install required libraries via Library Manager:

   * Adafruit GFX
   * Adafruit ILI9341

### Running

1. Connect your Pi Pico and TFT per wiring above.
2. Select the RP2040 board and port in IDE.
3. Upload the sketch.
4. Open Serial Monitor at **57600 baud** to view live event logs.
5. Toggle the slide switch to change modes.
6. Press pushbuttons to manually generate vehicles.

## Repository Structure

```
SmartTjunction/
├── diagram.json      # Wokwi circuit diagram
├── libraries.txt     # Required libraries
├── sketch.ino        # Main Arduino sketch
└── wokwi.toml        # Wokwi simulation config
```

## License

This project is released under the TAU License.

---
