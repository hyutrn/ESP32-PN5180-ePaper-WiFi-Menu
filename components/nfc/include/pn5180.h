// NAME: pn5180_types.h
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

#ifndef PN5180_CONFIG_H
#define PN5180_CONFIG_H

#include "sdkconfig.h"

/*
    SPI CONFIGURATION
*/
#ifndef CONFIG_PN5180_SPI_HOST
#define CONFIG_PN5180_SPI_HOST          SPI2_HOST  // ESP32 SPI2 or SPI3
#endif

#ifndef CONFIG_PN5180_SPI_CLOCK_HZ
#define CONFIG_PN5180_SPI_CLOCK_HZ      7000000    // 7 MHz max per datasheet
#endif

#ifndef CONFIG_PN5180_SPI_MODE
#define CONFIG_PN5180_SPI_MODE          0          // CPOL=0, CPHA=0
#endif

#ifndef CONFIG_PN5180_SPI_QUEUE_SIZE
#define CONFIG_PN5180_SPI_QUEUE_SIZE    4
#endif

/*
    PIN CONFIGURATION
*/
#ifndef CONFIG_PN5180_PIN_MISO
#define CONFIG_PN5180_PIN_MISO          -1         // Not used in half-duplex
#endif

#ifndef CONFIG_PN5180_PIN_MOSI
#define CONFIG_PN5180_PIN_MOSI          23
#endif

#ifndef CONFIG_PN5180_PIN_SCLK
#define CONFIG_PN5180_PIN_SCLK          18
#endif

#ifndef CONFIG_PN5180_PIN_NSS
#define CONFIG_PN5180_PIN_NSS           5
#endif

#ifndef CONFIG_PN5180_PIN_BUSY
#define CONFIG_PN5180_PIN_BUSY          4
#endif

#ifndef CONFIG_PN5180_PIN_RST
#define CONFIG_PN5180_PIN_RST           15
#endif

#ifndef CONFIG_PN5180_PIN_IRQ
#define CONFIG_PN5180_PIN_IRQ           16         // Optional
#endif

/*
    RF PERFORMANCE CONFIGURATION
*/
#ifndef CONFIG_PN5180_RX_GAIN_DEFAULT
#define CONFIG_PN5180_RX_GAIN_DEFAULT   0x07       // 42dB MAX
#endif

#ifndef CONFIG_PN5180_TX_POWER_DEFAULT
#define CONFIG_PN5180_TX_POWER_DEFAULT  0x0F       // Max power
#endif

#ifndef CONFIG_PN5180_ANTENNA_TUNING_DEFAULT
#define CONFIG_PN5180_ANTENNA_TUNING_DEFAULT 0x0088 // Typical for 50Ω antenna
#endif

#ifndef CONFIG_PN5180_IQ_THRESHOLD_DEFAULT
#define CONFIG_PN5180_IQ_THRESHOLD_DEFAULT 0x01    // Low threshold
#endif


/*
    TIMING CONFIGURATION (milliseconds)
*/
#ifndef CONFIG_PN5180_RESET_TIMEOUT_MS
#define CONFIG_PN5180_RESET_TIMEOUT_MS  100
#endif

#ifndef CONFIG_PN5180_SPI_TIMEOUT_MS
#define CONFIG_PN5180_SPI_TIMEOUT_MS    100
#endif

#ifndef CONFIG_PN5180_BUSY_TIMEOUT_MS
#define CONFIG_PN5180_BUSY_TIMEOUT_MS   100
#endif

#ifndef CONFIG_PN5180_DETECT_TIMEOUT_14443A_MS
#define CONFIG_PN5180_DETECT_TIMEOUT_14443A_MS  2
#endif

#ifndef CONFIG_PN5180_DETECT_TIMEOUT_14443B_MS
#define CONFIG_PN5180_DETECT_TIMEOUT_14443B_MS  2
#endif

#ifndef CONFIG_PN5180_DETECT_TIMEOUT_15693_MS
#define CONFIG_PN5180_DETECT_TIMEOUT_15693_MS   5
#endif

#ifndef CONFIG_PN5180_DATA_TIMEOUT_MS
#define CONFIG_PN5180_DATA_TIMEOUT_MS   20
#endif

/*
    BUFFER SIZES
*/
#ifndef CONFIG_PN5180_TX_BUFFER_SIZE
#define CONFIG_PN5180_TX_BUFFER_SIZE    1024       // Max 1KB
#endif

#ifndef CONFIG_PN5180_RX_BUFFER_SIZE
#define CONFIG_PN5180_RX_BUFFER_SIZE    1024       // Max 1KB
#endif

#ifndef CONFIG_PN5180_COMMAND_QUEUE_SIZE
#define CONFIG_PN5180_COMMAND_QUEUE_SIZE 10
#endif

/*
    DEBUG AND LOGGING CONFIGURATION
*/
#ifndef CONFIG_PN5180_DEBUG_LEVEL
#define CONFIG_PN5180_DEBUG_LEVEL       2          // 0=off, 1=error, 2=info, 3=debug
#endif

#ifndef CONFIG_PN5180_LOG_TAG
#define CONFIG_PN5180_LOG_TAG           "PN5180"
#endif

/*
    MULTI-PROTOCOL SCANNING CONFIGURATION
*/
#ifndef CONFIG_PN5180_SCAN_CYCLE_DELAY_MS
#define CONFIG_PN5180_SCAN_CYCLE_DELAY_MS 1
#endif

#ifndef CONFIG_PN5180_MAX_SCAN_PROTOCOLS
#define CONFIG_PN5180_MAX_SCAN_PROTOCOLS 4
#endif

/*
    POWER MANAGEMENT CONFIGURATION
*/
#ifndef CONFIG_PN5180_LPCD_ENABLED
#define CONFIG_PN5180_LPCD_ENABLED      0          // Low Power Card Detection
#endif

#ifndef CONFIG_PN5180_SLEEP_ENABLED
#define CONFIG_PN5180_SLEEP_ENABLED     1          // Sleep mode support
#endif

#endif // PN5180_CONFIG_H