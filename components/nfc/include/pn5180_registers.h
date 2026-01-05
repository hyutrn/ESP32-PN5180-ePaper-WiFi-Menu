// NAME: pn5180_registers.h
//
// DESC: NFC Communication with NXP Semiconductors PN5180 module for Arduino.
//
// Copyright (c) 2025 by Tran Trong Huy. All rights reserved.
//
// This file is part of the PN5180 library for the Arduino environment.
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// Lesser General Public License for more details.
//

#ifndef PN5180_REGISTERS_H
#define PN5180_REGISTERS_H

#ifdef __cplusplus
extern "C" {
#endif

/* 
    System Registers 
*/
#define PN5180_REG_SYSTEM_CONFIG          0x00
#define PN5180_REG_IRQ_ENABLE             0x01
#define PN5180_REG_IRQ_STATUS             0x02
#define PN5180_REG_ERROR_STATUS           0x03
#define PN5180_REG_STATUS                 0x04

/* 
    System configuration register (0x00) bit definitions 
*/
#define PN5180_SYSTEM_CONFIG_RESET        (1 << 0) // Software Reset (1=Reset, Auto-Clear)
#define PN5180_SYSTEM_CONFIG_POWER_DOWN   (1 << 1) // Enter Power Down Mode 
#define PN5180_SYSTEM_CONFIG_STANDBY      (1 << 2) // Enter Standby Mode
#define PN5180_SYSTEM_CONFIG_IRQ_POL      (1 << 3) // IRQ Pin Polarity (0=Active Low, 1=Active High)
#define PN5180_SYSTEM_CONFIG_IRQ_OD       (1 << 4) // IRQ Pin Output Type (0=Push-Pull, 1=Open-Drain)
#define PN5180_SYSTEM_CONFIG_CLOCK_SEL    (1 << 5) // Clock Selection (0=27.12 MHz, 1=13.56 MHz)

/* 
    IRQ Enable register (0x01) bit definitions 
*/
#define PN5180_IRQ_ENABLE_RF_FIELD_ACTIVE  (1 << 0) // RF Field Active IRQ
#define PN5180_IRQ_ENABLE_TX_DONE          (1 << 1) // Transmission complete IRQ
#define PN5180_IRQ_ENABLE_RX_DONE          (1 << 2) // Reception complete IRQ
#define PN5180_IRQ_ENABLE_RX_ERROR         (1 << 3) // Reception error IRQ
#define PN5180_IRQ_ENABLE_TX_ERROR         (1 << 4) // Transmission error IRQ
#define PN5180_IRQ_ENABLE_IDLE             (1 << 5) // IDLE State IRQ
#define PN5180_IRQ_ENABLE_GENERAL_ERROR    (1 << 6) // General error IRQ
#define PN5180_IRQ_ENABLE_WAKE_UP          (1 << 7) // Wake-up IRQ

/* 
    IRQ Status register (0x02) bit definitions 
*/
#define PN5180_IRQ_STATUS_RF_FIELD_ACTIVE  (1 << 0) // RF Field is active
#define PN5180_IRQ_STATUS_TX_DONE          (1 << 1) // Transmission complete
#define PN5180_IRQ_STATUS_RX_DONE          (1 << 2) // Reception complete
#define PN5180_IRQ_STATUS_RX_ERROR         (1 << 3) // Reception error occurred
#define PN5180_IRQ_STATUS_TX_ERROR         (1 << 4) // Transmission error occurred
#define PN5180_IQR_STATUS_IDLE             (1 << 5) // Device is idle
#define PN5180_IRQ_STATUS_GENERAL_ERROR    (1 << 6) // General error occurred
#define PN5180_IRQ_STATUS_WAKE_UP          (1 << 7) // Wake-up event 

/* 
    Error Status register (0x03) bit definitions (read-only) 
*/
#define PN5180_ERROR_PROTOCOL               (1 << 0) // Protocol error
#define PN5180_ERROR_CRC                    (1 << 1) // CRC error
#define PN5180_ERROR_PARITY                 (1 << 2) // Parity error
#define PN5180_ERROR_FRAMING                (1 << 3) // Framing error
#define PN5180_ERROR_COLLISION              (1 << 4) // Collision error
#define PN5180_ERROR_RX_BUFFER_OVERFLOW     (1 << 5) // RX buffer overflow error
#define PN5180_ERROR_RX_BUFFER_UNDERFLOW    (1 << 6) // RX buffer underflow error
// BIT7: Reserved

/* 
    Status register (0x04) bit definitions (read-only) 
*/
#define PN5180_STATUS_RF_FIELD      (1 << 0) // RF Field status (1=On)
#define PN5180_STATUS_TX_ACTIVE     (1 << 1) // Transmission active status (1=Transmitting)
#define PN5180_STATUS_RX_ACTIVE     (1 << 2) // Reception active status (1=Receiving)
#define PN5180_STATUS_IDLE          (1 << 3) // Idle status (1=Idle)
#define PN5180_STATUS_POWER_DOWN   (1 << 4) // Power down status (1=Powered Down)
// BIT5-BIT7: Reserved


/* 
    RF Control Registers 
*/
#define PN5180_REG_RF_CONTROL            0x05
#define PN5180_REG_RF_CONTROL_2          0x06
#define PN5180_REG_RF_CONTROL_3          0x07
#define PN5180_REG_RF_CONTROL_4          0x08
#define PN5180_REG_RF_CONTROL_5          0x09
#define PN5180_REG_RF_CONTROL_6          0x0A
#define PN5180_REG_RF_CONTROL_7          0x0B
#define PN5180_REG_RF_CONTROL_8          0x0C
#define PN5180_REG_RF_CONTROL_9          0x0D
#define PN5180_REG_RF_CONTROL_10         0x0E
#define PN5180_REG_RF_CONTROL_11         0x0F

/* 
    RF Control register (0x05) bit definitions 
*/
#define PN5180_RF_CONTROL_FIELD_ON       (1 << 7) // RF field enabled (1=ON)
// BIT6-BIT4: TX driver current control (DRV_CURRENT)
#define PN5180_RF_CONTROL_DRV_CURRENT_POS 4 
#define PN5180_RF_CONTROL_DRV_CURRENT_MASK (0x07 << PN5180_RF_CONTROL_DRV_CURRENT_POS)
// BIT3-BIT0: TX drier configuration (DRV_CONF)
#define PN5180_RF_CONTROL_DRV_MASK        0x0F

/* 
    RF Status Registers (0x10-0x19) 
*/
#define PN5180_REG_RF_STATUS              0x10
#define PN5180_REG_RF_STATUS_2            0x11
#define PN5180_REG_RF_STATUS_3            0x12
#define PN5180_REG_RF_STATUS_4            0x13
#define PN5180_REG_RF_STATUS_5            0x14
#define PN5180_REG_RF_STATUS_6            0x15
#define PN5180_REG_RF_STATUS_7            0x16
#define PN5180_REG_RF_STATUS_8            0x17
#define PN5180_REG_RF_STATUS_9            0x18
#define PN5180_REG_RF_STATUS_10           0x19

/* 
    RF_STATUS (0x10) bit definitions 
*/
#define PN5180_RF_STATUS_RSSI_MASK        0x1F      // Bits 4-0: RSSI value (5-bit)
#define PN5180_RF_STATUS_RF_FIELD_DET     (1 << 5)  // RF field detected
#define PN5180_RF_STATUS_AGC_LOCKED       (1 << 6)  // AGC locked
// BIT 7: Reserved

/* 
    Receiver configuration registers 
*/
#define PN5180_REG_RX_CONF1              0x1A
#define PN5180_REG_RX_CONF2              0x1B
#define PN5180_REG_RX_CONF3              0x1C
#define PN5180_REG_RX_CONF4              0x1D
#define PN5180_REG_RX_CONF5              0x1E

/* 
    RX_CONF1 (0x1A) bit definitions - CRITICAL FOR SENSITIVITY 
*/
// Bits 2-0: Gain selection (GAIN_SEL)
#define PN5180_RX_CONF1_GAIN_SEL_POS      0
#define PN5180_RX_CONF1_GAIN_SEL_MASK     (0x7 << PN5180_RX_CONF1_GAIN_SEL_POS)
#define PN5180_RX_GAIN_0DB                0x00      // 0 dB
#define PN5180_RX_GAIN_6DB                0x01      // 6 dB  
#define PN5180_RX_GAIN_12DB               0x02      // 12 dB
#define PN5180_RX_GAIN_18DB               0x03      // 18 dB
#define PN5180_RX_GAIN_24DB               0x04      // 24 dB
#define PN5180_RX_GAIN_30DB               0x05      // 30 dB
#define PN5180_RX_GAIN_36DB               0x06      // 36 dB
#define PN5180_RX_GAIN_42DB               0x07      // 42 dB MAX
// Bits 5-3: Bandwidth selection (BW_SEL)
#define PN5180_RX_CONF1_BW_SEL_POS        3
#define PN5180_RX_CONF1_BW_SEL_MASK       (0x7 << PN5180_RX_CONF1_BW_SEL_POS)
#define PN5180_RX_BW_1_8MHZ               0x00      // 1.8 MHz (default)
#define PN5180_RX_BW_1_2MHZ               0x01      // 1.2 MHz
#define PN5180_RX_BW_0_9MHZ               0x02      // 0.9 MHz
#define PN5180_RX_BW_0_6MHZ               0x03      // 0.6 MHz
#define PN5180_RX_BW_0_45MHZ              0x04      // 0.45 MHz
#define PN5180_RX_BW_0_3MHZ               0x05      // 0.3 MHz
#define PN5180_RX_BW_0_225MHZ             0x06      // 0.225 MHz
#define PN5180_RX_BW_0_15MHZ              0x07      // 0.15 MHz

/* 
    RX_CONF3 (0x1C) bit definitions - CRITICAL FOR DETECTION THRESHOLD 
*/
// Bits 3-0: Minimum modulation depth (MIN_MOD_DEPTH)
#define PN5180_RX_CONF3_MIN_MOD_DEPTH_POS 0
#define PN5180_RX_CONF3_MIN_MOD_DEPTH_MASK (0xF << PN5180_RX_CONF3_MIN_MOD_DEPTH_POS)
// Bits 6-4: I/Q detection threshold (IQ_DET_THR)
#define PN5180_RX_CONF3_IQ_DET_THR_POS    4
#define PN5180_RX_CONF3_IQ_DET_THR_MASK   (0x7 << PN5180_RX_CONF3_IQ_DET_THR_POS)

/* 
    Transmitter configuration registers (0x1F-0x2B) 
*/
#define PN5180_REG_TX_CONF1               0x1F
#define PN5180_REG_TX_CONF2               0x20
#define PN5180_REG_TX_CONF3               0x21
#define PN5180_REG_TX_CONF4               0x22
#define PN5180_REG_TX_CONF5               0x23
#define PN5180_REG_TX_CONF6               0x24
#define PN5180_REG_TX_CONF7               0x25
#define PN5180_REG_TX_CONF8               0x26
#define PN5180_REG_TX_CONF9               0x27
#define PN5180_REG_TX_CONF10              0x28
#define PN5180_REG_TX_CONF11              0x29
#define PN5180_REG_TX_CONF12              0x2A
#define PN5180_REG_TX_CONF13              0x2B

/* 
    TX_CONF1 (0x1F) bit definitions 
*/
// Bits 1-0: Bit rate selection (BITRATE_SEL)
#define PN5180_TX_CONF1_BITRATE_SEL_POS   0
#define PN5180_TX_CONF1_BITRATE_SEL_MASK  (0x3 << PN5180_TX_CONF1_BITRATE_SEL_POS)
#define PN5180_TX_BITRATE_106KBPS         0x00      // 106 kbps
#define PN5180_TX_BITRATE_212KBPS         0x01      // 212 kbps
#define PN5180_TX_BITRATE_424KBPS         0x02      // 424 kbps
#define PN5180_TX_BITRATE_848KBPS         0x03      // 848 kbps
// Bits 3-2: Modulation type (MOD_TYPE)
#define PN5180_TX_CONF1_MOD_TYPE_POS      2
#define PN5180_TX_CONF1_MOD_TYPE_MASK     (0x3 << PN5180_TX_CONF1_MOD_TYPE_POS)
#define PN5180_TX_MOD_100_PERCENT_ASK     0x00      // 100% ASK (ISO14443A)
#define PN5180_TX_MOD_10_PERCENT_ASK      0x01      // 10% ASK (ISO14443B/ISO15693)
// Bits 7-6: Preamble length (PREAMBLE_LEN)
#define PN5180_TX_CONF1_PREAMBLE_LEN_POS  6
#define PN5180_TX_CONF1_PREAMBLE_LEN_MASK (0x3 << PN5180_TX_CONF1_PREAMBLE_LEN_POS)

/* 
    Protocol-specific TX drivers configuration registers (0x2C-0x2F) 
*/
#define PN5180_REG_ISO14443A_TX_DRIVER    0x2C  // CRITICAL FOR TX POWER
#define PN5180_REG_ISO14443B_TX_DRIVER    0x2D
#define PN5180_REG_ISO15693_TX_DRIVER     0x2E
#define PN5180_REG_NFCIP1_TX_DRIVER       0x2F

/* 
    TX Driver register bit definitions (0x2C-0x2F) 
*/
// Bits 3-0: Driver strength (DRV_STRENGTH)
#define PN5180_TX_DRIVER_STRENGTH_MASK    0x0F
#define PN5180_TX_DRIVER_MIN              0x00      // Minimum power
#define PN5180_TX_DRIVER_MAX              0x0F      // Maximum power (~1.5W)

/* 
    Buffer management registers (0x40-0x45) 
*/
#define PN5180_REG_TX_DATA                0x40  // TX buffer pointer (auto-increment)
#define PN5180_REG_RX_DATA                0x41  // RX buffer pointer (auto-increment)
#define PN5180_REG_TX_LENGTH_LSB          0x42  // TX data length LSB
#define PN5180_REG_TX_LENGTH_MSB          0x43  // TX data length MSB
#define PN5180_REG_FIFO_CONTROL           0x44
#define PN5180_REG_FIFO_STATUS            0x45

/* 
    FIFO_CONTROL (0x44) bit definitions 
*/
#define PN5180_FIFO_CTRL_TX_RESET         (1 << 0)  // Reset TX FIFO
#define PN5180_FIFO_CTRL_RX_RESET         (1 << 1)  // Reset RX FIFO
#define PN5180_FIFO_CTRL_FIFO_ENABLE      (1 << 2)  // Enable FIFO
// Bits 3-7: Reserved

/* 
    FIFO_STATUS (0x45) bit definitions (Read-only) 
*/
#define PN5180_FIFO_STATUS_TX_EMPTY       (1 << 0)  // TX FIFO empty
#define PN5180_FIFO_STATUS_TX_FULL        (1 << 1)  // TX FIFO full
#define PN5180_FIFO_STATUS_RX_EMPTY       (1 << 2)  // RX FIFO empty
#define PN5180_FIFO_STATUS_RX_FULL        (1 << 3)  // RX FIFO full
// Bits 4-7: Reserved

/* 
    Timing registers (0x46-0x47) 
*/
#define PN5180_REG_TX_TIMER_CONFIG        0x46
#define PN5180_REG_RX_TIMER_CONFIG        0x47

/* 
    Power management registers (0x48-0x49) 
*/
#define PN5180_REG_POWER_DOWN             0x48
#define PN5180_REG_WAKE_UP                0x49
/* 
    Power_DOWN register bit definitions (0x48) 
*/
#define PN5180_POWER_DOWN_ENTER           (1 << 0)  // Enter power down mode
#define PN5180_POWER_DOWN_WAKE_TIMER_EN   (1 << 1)  // Enable wake-up timer
// Bits 2-7: Reserved

/* 
    Antena control registers (0x4A-0x4C) 
*/
#define PN5180_REG_ANTENNA_CTRL           0x4A  // CRITICAL FOR ANTENNA TUNING
#define PN5180_REG_ANTENNA_TUNING_LSB     0x4B
#define PN5180_REG_ANTENNA_TUNING_MSB     0x4C

/* 
    ANTENNA_CTRL (0x4A) bit definitions 
*/
// Bits 3-0: Antenna tuning capacitance (TUNING_CAP)
#define PN5180_ANTENNA_CTRL_TUNING_CAP_POS 0
#define PN5180_ANTENNA_CTRL_TUNING_CAP_MASK (0xF << PN5180_ANTENNA_CTRL_TUNING_CAP_POS)
// Bits 7-4: Matching network configuration (MATCH_NET)
#define PN5180_ANTENNA_CTRL_MATCH_NET_POS 4
#define PN5180_ANTENNA_CTRL_MATCH_NET_MASK (0xF << PN5180_ANTENNA_CTRL_MATCH_NET_POS)

/* 
    CRC Configuration Registers (0x4D) 
*/
#define PN5180_REG_CRC_CONFIG           0x4D

/* 
    CRC Configuration (0x4D) bit definitions 
*/
#define PN5180_CRC_CONFIG_TX_ENABLE       (1 << 0)  // Enable TX CRC generation
#define PN5180_CRC_CONFIG_RX_ENABLE       (1 << 1)  // Enable RX CRC checking
// Bits 3-2: CRC type selection (CRC_TYPE)
#define PN5180_CRC_CONFIG_TYPE_POS        2
#define PN5180_CRC_CONFIG_TYPE_MASK       (0x3 << PN5180_CRC_CONFIG_TYPE_POS)
#define PN5180_CRC_TYPE_ISO14443A         0x00      // ISO14443A CRC
#define PN5180_CRC_TYPE_ISO14443B         0x01      // ISO14443B CRC
#define PN5180_CRC_TYPE_ISO15693          0x02      // ISO15693 CRC
// Bits 4-7: Reserved

/*
    Protocol-specific RX Configuration Registers (0x52-0x59)
*/
#define PN5180_REG_ISO14443A_RX_CONF1     0x52
#define PN5180_REG_ISO14443A_RX_CONF2     0x53
#define PN5180_REG_ISO14443B_RX_CONF1     0x54
#define PN5180_REG_ISO14443B_RX_CONF2     0x55
#define PN5180_REG_ISO15693_RX_CONF1      0x56
#define PN5180_REG_ISO15693_RX_CONF2      0x57
#define PN5180_REG_NFCIP1_RX_CONF1        0x58
#define PN5180_REG_NFCIP1_RX_CONF2        0x59

/*
    Test registers (0x60-0x7F) - Manufacturing only
*/
#define PN5180_REG_TEST_1                 0x60
#define PN5180_REG_TEST_2                 0x61
#define PN5180_REG_TEST_3                 0x62
#define PN5180_REG_TEST_4                 0x63
#define PN5180_REG_TEST_5                 0x64
#define PN5180_REG_TEST_6                 0x65
#define PN5180_REG_TEST_7                 0x66
#define PN5180_REG_TEST_8                 0x67
#define PN5180_REG_TEST_9                 0x68
#define PN5180_REG_TEST_10                0x69
#define PN5180_REG_TEST_11                0x6A
#define PN5180_REG_TEST_12                0x6B
#define PN5180_REG_TEST_13                0x6C
#define PN5180_REG_TEST_14                0x6D
#define PN5180_REG_TEST_15                0x6E
#define PN5180_REG_TEST_16                0x6F
#define PN5180_REG_TEST_17                0x70
#define PN5180_REG_TEST_18                0x71
#define PN5180_REG_TEST_19                0x72
#define PN5180_REG_TEST_20                0x73
#define PN5180_REG_TEST_21                0x74
#define PN5180_REG_TEST_22                0x75
#define PN5180_REG_TEST_23                0x76
#define PN5180_REG_TEST_24                0x77
#define PN5180_REG_TEST_25                0x78
#define PN5180_REG_TEST_26                0x79
#define PN5180_REG_TEST_27                0x7A
#define PN5180_REG_TEST_28                0x7B
#define PN5180_REG_TEST_29                0x7C
#define PN5180_REG_TEST_30                0x7D
#define PN5180_REG_TEST_31                0x7E
#define PN5180_REG_TEST_32                0x7F

/*
    Reversed Registers (0x80-0xFF) - Do not use
*/

/*
    Memory addresses (EEPROM)
*/
#define PN5180_EEPROM_DIE_IDENTIFIER      0x00
#define PN5180_EEPROM_PRODUCT_VERSION     0x10
#define PN5180_EEPROM_FIRMWARE_VERSION    0x12
#define PN5180_EEPROM_IRQ_PIN_CONFIG      0x1A
#define PN5180_EEPROM_LPCD_REF_VALUE      0x34  // Low Power Card Detection reference
#define PN5180_EEPROM_LPCD_FIELD_ON_TIME  0x36
#define PN5180_EEPROM_DPC_XI              0x5C  // Damping circuit setting

/*
    Common configuration values
*/
#define PN5180_DEFAULT_RX_GAIN            PN5180_RX_GAIN_42DB      // Max sensitivity
#define PN5180_DEFAULT_TX_POWER           0x0F                     // Max power
#define PN5180_DEFAULT_ANTENNA_TUNING     0x88                     // Typical 50Ω matching
#define PN5180_DEFAULT_MOD_DEPTH          0x02                     // 2% minimum modulation
#define PN5180_DEFAULT_IQ_THRESHOLD       0x01                     // Low detection threshold

/*
    SPI Commands
*/
#define PN5180_SPI_WRITE_MASK             0x80                    // MSB=1 for write
#define PN5180_SPI_READ_MASK              0x00                    // MSB=0 for read


#ifdef __cplusplus
}
#endif

#endif // PN5180_REGISTERS_H