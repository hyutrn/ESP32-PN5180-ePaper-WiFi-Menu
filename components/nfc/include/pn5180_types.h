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

#ifndef PN5180_TYPES_H
#define PN5180_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
    ERROR CODES
*/
typedef enum {
    PN5180_OK = 0,
    PN5180_ERR_INVALID_ARG = -1,
    PN5180_ERR_TIMEOUT = -2,
    PN5180_ERR_CRC = -3,
    PN5180_ERR_AUTH = -4,
    PN5180_ERR_PROTOCOL = -5,
    PN5180_ERR_BUFFER = -6,
    PN5180_ERR_SPI = -7,
    PN5180_ERR_NO_TAG = -8,
    PN5180_ERR_MULTIPLE_TAGS = -9,
    PN5180_ERR_HARDWARE = -10,
    PN5180_ERR_NOT_INIT = -11,
    PN5180_ERR_BUSY = -12,
    PN5180_ERR_RF_FIELD = -13,
    PN5180_ERR_EEPROM = -14,
    PN5180_ERR_UNSUPPORTED = -15
} pn5180_error_t;

/*
    PROTOCOL TYPES
*/
typedef enum {
    PN5180_PROTOCOL_ISO14443A = 0,  // MIFARE, NTAG, etc.
    PN5180_PROTOCOL_ISO14443B,      // Calypso, etc.
    PN5180_PROTOCOL_ISO15693,       // ICODE, TagIT, etc.
    PN5180_PROTOCOL_NFCIP1,         // Peer-to-Peer
    PN5180_PROTOCOL_FELICA,         // FeliCa (optional)
    PN5180_PROTOCOL_COUNT
} pn5180_protocol_t;

/*
    CARD INFORMATION
*/
typedef struct {
    uint8_t uid[10];           // UID (4, 7, or 10 bytes)
    uint8_t uid_len;           // Actual UID length
    pn5180_protocol_t protocol;
    uint8_t sak;               // Select Acknowledge (ISO14443A)
    uint16_t atqa;             // Answer To Request (ISO14443A)
    uint8_t dsfid;             // Data Storage Format Identifier (ISO15693)
    uint8_t afi;               // Application Family Identifier (ISO15693)
    uint16_t block_size;       // Block size in bytes
    uint16_t block_count;      // Number of blocks
    uint32_t timestamp;        // Detection timestamp (ms)
    uint8_t rssi;              // Received Signal Strength Indicator
} pn5180_card_info_t;

/*
    PIN CONFIGURATION
*/
typedef struct {
    int miso_pin;      // Master In Slave Out (optional for PN5180)
    int mosi_pin;      // Master Out Slave In
    int sclk_pin;      // Serial Clock
    int nss_pin;       // Slave Select (Chip Select)
    int busy_pin;      // BUSY signal (input)
    int rst_pin;       // Reset (output)
    int irq_pin;       // Interrupt (input, optional)
} pn5180_pin_config_t;

/*
    RF CONFIGURATION
*/
typedef struct {
    uint8_t rx_gain;           // Receiver gain (0-7 = 0-42dB)
    uint8_t tx_power;          // Transmit power (0x00-0x0F)
    uint8_t modulation_depth;  // Minimum modulation depth
    uint8_t iq_threshold;      // I/Q detection threshold
    uint16_t antenna_tuning;   // Antenna tuning value
    uint8_t crc_enabled : 1;   // CRC enable/disable
    uint8_t auto_rf_control : 1; // Automatic RF field control
    uint8_t lpcd_enabled : 1;  // Low Power Card Detection
} pn5180_rf_config_t;

/*
    PROTOCOL CONFIGURATION
*/
typedef struct {
    pn5180_protocol_t protocol;
    uint8_t tx_driver_reg;     // Register address for TX driver
    uint8_t tx_power;          // Power level for this protocol
    uint8_t rx_gain;           // Receiver gain for this protocol
    uint16_t detect_timeout_ms; // Detection timeout
    uint16_t data_timeout_ms;  // Data transfer timeout
    const uint8_t *detect_cmd; // Detection command
    uint8_t detect_cmd_len;    // Command length
} pn5180_protocol_config_t;

/*
    SCAN MODES
*/
typedef enum {
    PN5180_SCAN_MODE_SINGLE,    // Scan one protocol at a time
    PN5180_SCAN_MODE_MULTI,     // Multi-protocol scanning
    PN5180_SCAN_MODE_CONTINUOUS // Continuous scanning
} pn5180_scan_mode_t;

/*
    PN5180 DEVICE HANDLE
*/
typedef struct pn5180_dev_t pn5180_dev_t;

/*
    CALLBACK TYPES
*/
typedef void (*pn5180_card_callback_t)(pn5180_card_info_t *card, void *user_data);
typedef void (*pn5180_error_callback_t)(pn5180_error_t error, void *user_data);
typedef void (*pn5180_log_callback_t)(const char *message, void *user_data);

#ifdef __cplusplus
}
#endif

#endif // PN5180_TYPES_H