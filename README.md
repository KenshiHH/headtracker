# RP2040 BNO085 Head Tracker with USB Joystick

An **Arduino sketch** for the **Raspberry Pi Pico (RP2040 Arduino core)** that reads orientation data from the **BNO085 IMU** and converts head movement into a **USB HID joystick** input.  

This allows your head movements (yaw, pitch, roll) to control games or applications that accept joystick input.  

---

## Features

- Reads BNO085 sensor via **I2C**.
- Uses the **built-in sensor fusion** from BNO085 for accurate orientation.
- Outputs **Yaw, Pitch, Roll** via Serial for debugging.
- Converts head movements to **USB HID joystick axes**:
  - **Yaw** → X-axis
  - **Pitch** → Y-axis
  - **Roll** → Z-axis (or can be mapped to buttons)
- Supports **view reset** via serial command (`reset`).

---

## Hardware

**Required components:**

- Raspberry Pi Pico (RP2040)
- BNO085 IMU sensor
- Connecting wires

**Wiring (I2C)**:

| BNO085 Pin | Pico Pin |
|------------|----------|
| VIN        | 3V3      |
| GND        | GND      |
| SDA        | GP4      |
| SCL        | GP5      |

---

## Software

**Required Libraries** (install via Arduino Library Manager):

- **Arduino IDE** (for compiling and uploading the sketch)
- [Adafruit BNO08x](https://github.com/adafruit/Adafruit_BNO08x)
- [Adafruit TinyUSB Library](https://github.com/adafruit/Adafruit_TinyUSB) (for HID joystick support)

---

## Installation

1. Install the required libraries in the Arduino IDE.
2. Connect the BNO085 to your Raspberry Pi Pico according to the wiring table.
3. Upload the sketch to the Pico.
4. Open the Serial Monitor at **115200 baud** for debugging and optional commands.
5. Your Pico will enumerate as a USB joystick, sending head movement data to your PC.

---

## Usage

- Move your head to control the joystick:
  - **Yaw (left/right)** → X-axis
  - **Pitch (up/down)** → Y-axis
  - **Roll (left/right lean)** → Z-axis or buttons
- To reset the view (center head position), type in the Serial Monitor:
