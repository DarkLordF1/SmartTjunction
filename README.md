# SmartTjunction

A cooperative‐multitasking simulation and control system for a T‑junction traffic light network, implemented on a Raspberry Pi Pico (or compatible RP2040 board) and an ILI9341 TFT display.

---

## Table of Contents

* [Overview](#overview)
* [Features](#features)
* [Hardware Requirements](#hardware-requirements)
* [Wiring](#wiring)
* [Software Requirements](#software-requirements)
* [Installation](#installation)
* [Usage](#usage)
* [Repository Structure](#repository-structure)
* [License & Credits](#license--credits)

---

## Overview

SmartTjunction transforms a basic T‑junction into a “smart” intersection by:

1. **Vehicle Generation** in two modes:

   * **Manual**: via pushbuttons.
   * **Random**: per‑lane Markov process (steady vs. variable intervals).
2. **Round‑Robin Scheduling**: cycles each of three traffic lights through red, yellow (0.5 s), and green (1–7 s).
3. **Flow Control**: allows one vehicle per second to cross on green.
4. **Dynamic Optimization**: every 30 s, adjusts green durations based on queued vs. thrown vehicles.
5. **Live Dashboard**: displays current mode, light status, queue lengths, pattern, and thrown count on an ILI9341 TFT.
6. **Serial Logging**: timestamps all events (light changes, vehicle events, optimizations) at 57600 baud.

---

## Features

* **3 Traffic Lights** (TL1, TL2, TL3) with exclusive color states.
* **Dual Vehicle‑Generation**: pushbutton vs. Markov‑chain random.
* **Queue Management**: up to 4 cars/lane; overflow counted as “thrown.”
* **Graphical UI**: textual + icon‑based traffic lights rendered on TFT.
* **Real‑Time Logs**: formatted entries like:

  ```
  12345 ms, system, TL1 → GREEN
  12346 ms, vehicle 5, generated, L1:1,L2:0,L3:0
  13345 ms, system, crossed from lane 1, vehicle 5
  ```

---

## Hardware Requirements

* **Raspberry Pi Pico** (or RP2040 board)
* **3× RGB LEDs** (one per traffic light)
* **ILI9341 TFT Display**
* **3× Pushbuttons**
* **1× Slide Switch**
* **Resistors, wires, breadboard**

---

## Wiring

|      Signal | Pico Pin | Device                                     |
| ----------: | :------- | :----------------------------------------- |
|     TL1 Red | GP10     | RGB LED 1 (Red Anode)                      |
|   TL1 Green | GP9      | RGB LED 1 (Green Anode)                    |
|     TL2 Red | GP7      | RGB LED 2 (Red Anode)                      |
|   TL2 Green | GP6      | RGB LED 2 (Green Anode)                    |
|     TL3 Red | GP4      | RGB LED 3 (Red Anode)                      |
|   TL3 Green | GP3      | RGB LED 3 (Green Anode)                    |
|    Button 1 | GP14     | Pushbutton 1                               |
|    Button 2 | GP13     | Pushbutton 2                               |
|    Button 3 | GP12     | Pushbutton 3                               |
| Mode Switch | GP11     | Slide Switch (HIGH = Manual, LOW = Random) |
|      TFT CS | GP22     | ILI9341 CS                                 |
|      TFT DC | GP20     | ILI9341 DC                                 |
|     TFT RST | GP21     | ILI9341 RST                                |
|    TFT MOSI | GP19     | SPI MOSI                                   |
|     TFT SCK | GP18     | SPI SCK                                    |
|    TFT MISO | GP16     | SPI MISO                                   |

---

## Software Requirements

* Arduino IDE (or VS Code + PlatformIO) with RP2040 support
* Adafruit GFX library
* Adafruit ILI9341 library

---

## Installation

```bash
git clone https://github.com/DarkLordF1/SmartTjunction.git
cd SmartTjunction
```

1. Open `sketch.ino` in your IDE.
2. Install libraries via Library Manager:

   * Adafruit GFX
   * Adafruit ILI9341
3. Adjust pin definitions if needed.

---

## Usage

1. Wire hardware per above table.
2. Select RP2040 board & correct port in IDE.
3. Upload `sketch.ino`.
4. Open Serial Monitor at **57600 baud** for live logs.
5. Toggle slide switch for mode; press buttons to add vehicles.

---

## Repository Structure

```
SmartTjunction/
├── diagram.json      # Wokwi circuit diagram
├── libraries.txt     # Required libraries
├── sketch.ino        # Main Arduino sketch
└── wokwi.toml        # Wokwi simulation config
```

---

## License & Credits

This project is licensed under the **Tel Aviv University (TAU) License**. See [LICENSE](LICENSE) for full terms.

<p align="center">
  <img src="https://en-engineering.tau.ac.il/sites/engineering-english.tau.ac.il/files/TAU_facultot_logos-01-handasa_0.png" alt="Tel Aviv University" height="72" width="212">
</p>
