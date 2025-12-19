# ESP32-PN5180-ePaper

## 📋 Overview
An embedded UI project using ESP32, PN5180 NFC reader, and a 2.66-inch 3-color ePaper display (Black / White / Red).
The system provides a menu-driven interface, NFC card UID display, and WiFi configuration demo, optimized for low power and minimal screen flicker.
## ✨ Features
ESP32 Devkit
PN5180 NFC Reader
- ISO14443A
- Reads 4 / 7 / 10-byte UID
2.66” Pico ePaper Display
- Resolution: 296 × 152
- Colors: Black / White / Red
- Partial refresh supported
### File Structure
- PN5180-NFC-Reader/
- ├── src/
- │   └── PN5180_reader.ino  # Main
- ├── inc/
- │   └──PN5180 Library        # Library for PN5180
- └── README.md                # This file
## 🧱 Hardware Requirements
- PN5180 NFC Reader Module
- ESP32 Development Board (This project using ESP32 Devkit)
- ISO14443A NFC Cards/Tags compliant, includes: Mifare Ultralight/NTAG, Mifare Classic, Mifare Plus, JCOP31/41

Wiring Diagram
PN5180 Pin	|   ESP32 Pin
-     NSS     |    GPIO 16
-     BUSY    |    GPIO 5
-     RST     |    GPIO 17
-     SCK     |    GPIO 18
-     MOSI    |    GPIO 23
-     MISO    |    GPIO 19
-     VCC     |    3.3V
-     GND     |    GND
- **Note**: Never swap NSS pin (16) and Busy pin (5)
- <img width="3487" height="2092" alt="esp32-pn5180" src="https://github.com/user-attachments/assets/7ba69ed8-1a05-4073-891c-9e414e22d4fa" />
ePaper     |  ESP32 Pin
- CS       |  15
- DC       |  2
- RST      |  4
- BUSY     |  13
- MOSI     |  23
- SCK      |  18
- VCC      |  3.3V
- GND      |  GND
![Pico-ePaper-2 66-details-inter](https://github.com/user-attachments/assets/2b82aa8e-88dd-409a-a38c-80b17fe02ed1)
## Software Installation
Install Arduino IDE (version 1.8.x or later)
Install PN5180 Library by include .zip file:
- Search for "PN5180" and install by tueddy (link: https://github.com/tueddy/PN5180-Library/blob/master/PN5180.h)
- Clone or download this repository 
- Open Arduino, add zip file to library
Install GxEPD2 Library via Arduino Library Manager
Open PN5180_reader.ino in Arduino IDE
Select board (ESP32 Dev Module) and correct COM port
Upload the sketch
## Contributing

Contributions are welcome! Please open an issue or submit a pull request.
