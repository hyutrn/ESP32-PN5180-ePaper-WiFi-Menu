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
| VSYS      | 3.3V      |
| GND       | GND       |

![Pico ePaper 2.66](https://github.com/user-attachments/assets/2b82aa8e-88dd-409a-a38c-80b17fe02ed1)

---

## 💻 Software Installation



## Demo
![6791399d5a75d52b8c64](https://github.com/user-attachments/assets/10b95a58-8ade-43a8-9803-0473d361a367)
![45954d942e7ca122f86d](https://github.com/user-attachments/assets/d2a76eb7-0d0f-467e-8b35-30df2f79ed15)
![29694a63298ba6d5ff9a](https://github.com/user-attachments/assets/af63d368-1f55-4ff2-8d95-df9bce0d332a)
![8284a782c46a4b34127b](https://github.com/user-attachments/assets/c9448f1c-d9b4-4eef-897f-dac34f88eff3)
![4cb47fba1c52930cca43](https://github.com/user-attachments/assets/cbe535f2-a935-41f9-8cc9-a6a66027b126)

---

## 🤝 Contributing
Contributions are welcome 🎉  
Please open an **issue** or submit a **pull request** if you have improvements or fixes.

