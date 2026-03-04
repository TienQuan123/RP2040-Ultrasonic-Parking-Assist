# RP2040-Ultrasonic-Parking-Assist
## 1. Project Overview
A full-system embedded project designed to measure distance using the US-100 ultrasonic sensor and the RP2040 microcontroller. The system features a custom-designed hardware expansion board, C/C++ bare-metal firmware with digital filtering, and a C# WinForms GUI for real-time visualization and serial communication testing.

## 2. System Architecture
* **Hardware Design:** * Custom PCB expansion board designed in **Altium Designer**.
  * Integrated **6N137 Optocouplers** for MCU isolation and protection.
  * Dedicated ground planes (GND_USB and GND_PICO) to minimize electrical noise.
* **Firmware (C/C++):** * Implemented dual Software UART (Timer-interrupt-based for sensor interfacing).
  * Applied a **10-sample Moving Average Filter** to stabilize distance readings.
  * Formatted serial data frames with an 8-bit **CRC checksum** (`@DIST_[value]_#[CRC]`) to ensure data integrity.
* **Software (C# WinForms):** * Developed a PC application to parse UART frames, visualize distance, and perform a 1000-packet stress test (achieving 0% packet loss).

## 3. Results
* Achieved highly stable distance measurements ranging from 5cm to 30cm with an error rate of < 1.05%.
* Validated robust UART communication with zero frame drops during high-frequency transmission.
