# ESP32-PN5180-ePaper

## 📋 Overview
An embedded UI project using **ESP32**, **PN5180 NFC reader**, and a **2.66-inch 3-color ePaper display** (Black / White / Red).

The system provides:
- Menu-driven UI
- NFC card UID display
- WiFi configuration demo  
- Optimized for **low power consumption** and **minimal ePaper flicker**

---

## ✨ Features

### 🔌 Hardware
- **ESP32 DevKit**
- **PN5180 NFC Reader**
  - ISO14443A
  - Reads 4 / 7 / 10-byte UID
- **2.66” Pico ePaper Display**
  - Resolution: 296 × 152
  - Colors: Black / White / Red
  - Partial refresh supported

---

## 📁 File Structure
- ESP32-PN5180-ePaper/
- ├── src/
- │ └──nfc_display.ino # Main application
- ├── inc/
- │ └── PN5180/ # PN5180 library
- └── README.md # Documentation


---

## 🧱 Hardware Requirements
- ESP32 Development Board (ESP32 DevKit used in this project)
- PN5180 NFC Reader Module
- 2.66” Pico ePaper Display
- ISO14443A NFC Cards/Tags, including:
  - MIFARE Ultralight / NTAG
  - MIFARE Classic
  - MIFARE Plus
  - JCOP31 / JCOP41

---

## 🔧 Wiring Diagram

### 🔹 PN5180 → ESP32

| PN5180 Pin | ESP32 Pin |
|-----------|-----------|
| NSS       | GPIO 16   |
| BUSY      | GPIO 5    |
| RST       | GPIO 17   |
| SCK       | GPIO 18   |
| MOSI      | GPIO 23   |
| MISO      | GPIO 19   |
| VCC       | 3.3V      |
| GND       | GND       |

⚠ **Important:**  
**Never swap NSS (GPIO16) and BUSY (GPIO5)** – this will cause PN5180 communication failure.

![ESP32 PN5180 Wiring](https://github.com/user-attachments/assets/7ba69ed8-1a05-4073-891c-9e414e22d4fa)

---

### 🔹 ePaper → ESP32

| ePaper Pin | ESP32 Pin |
|-----------|-----------|
| CS        | GPIO 15   |
| DC        | GPIO 2    |
| RST       | GPIO 4    |
| BUSY      | GPIO 13   |
| MOSI      | GPIO 23   |
| SCK       | GPIO 18   |
| VSY       | 3.3V      |
| GND       | GND       |

![Pico ePaper 2.66](https://github.com/user-attachments/assets/2b82aa8e-88dd-409a-a38c-80b17fe02ed1)

---

## 💻 Software Installation

### 1️⃣ Arduino IDE
- Install **Arduino IDE 1.8.x or later**
- Install **ESP32 Board Support**

---

### 2️⃣ PN5180 Library
- Download PN5180 library by **tueddy**  
  🔗 https://github.com/tueddy/PN5180-Library
- Add library via: Sketch → Include Library → Add .ZIP Library

---

### 3️⃣ ePaper Library
- Install **GxEPD2** via **Arduino Library Manager**

---

### 4️⃣ Build & Upload
1. Open `nfc_display.ino`
2. Select:
 - Board: **ESP32 Dev Module**
 - Correct **COM Port**
3. Click **Upload**

---

## 🤝 Contributing
Contributions are welcome 🎉  
Please open an **issue** or submit a **pull request** if you have improvements or fixes.

