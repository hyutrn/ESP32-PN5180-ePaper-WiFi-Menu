// NAME: pn5180_private.h
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

#ifndef PN5180_PRIVATE_H
#define PN5180_PRIVATE_H

#include "pn5180_registers.h"
#include "pn5180_types.h"
#include "pn5180_config.h"
#include "pn5180.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
    DEVICE STATES
*/
typedef enum {
    DEVICE_STATE_UNINITIALIZED,
    DEVICE_STATE_RESETTING,
    DEVICE_STATE_IDLE,
    DEVICE_STATE_CONFIGURING,
    DEVICE_STATE_SCANNING,
    DEVICE_STATE_TRANSMITTING,
    DEVICE_STATE_RECEIVING,
    DEVICE_STATE_PROCESSING,
    DEVICE_STATE_ERROR,
    DEVICE_STATE_SLEEP,
    DEVICE_STATE_WAKING_UP
} pn5180_device_state_t;

/*
    INTERNAL DEVICE STRUCTURE
*/
struct pn5180_dev_t {
    // ===== SPI Configuration =====
    spi_host_device_t spi_host;
    spi_device_handle_t spi_device;
    uint32_t spi_clock_hz;
    
    // ===== GPIO Pins =====
    int pin_miso;
    int pin_mosi;
    int pin_sclk;
    int pin_nss;
    int pin_busy;
    int pin_rst;
    int pin_irq;
    
    // ===== Device State =====
    pn5180_device_state_t state;
    uint32_t state_timestamp;
    uint8_t error_count;
    
    // ===== RF Configuration =====
    pn5180_rf_config_t rf_config;
    pn5180_protocol_config_t protocol_configs[PN5180_PROTOCOL_COUNT];
    
    // ===== Scanning Control =====
    bool scanning_enabled;
    uint8_t enabled_protocols;  // Bitmask
    uint8_t current_protocol_index;
    uint32_t last_scan_time;
    
    // ===== Callbacks =====
    pn5180_card_callback_t card_callback;
    pn5180_error_callback_t error_callback;
    pn5180_log_callback_t log_callback;
    void *callback_user_data;
    
    // ===== Buffers =====
    uint8_t tx_buffer[CONFIG_PN5180_TX_BUFFER_SIZE];
    uint8_t rx_buffer[CONFIG_PN5180_RX_BUFFER_SIZE];
    uint16_t tx_length;
    uint16_t rx_length;
    
    // ===== Synchronization =====
    SemaphoreHandle_t spi_mutex;
    SemaphoreHandle_t device_mutex;
    TaskHandle_t state_task_handle;
    QueueHandle_t command_queue;
    
    // ===== Statistics =====
    uint32_t total_scans;
    uint32_t cards_detected;
    uint32_t crc_errors;
    uint32_t timeout_errors;
    uint32_t protocol_errors;
    
    // ===== EEPROM Data =====
    uint32_t product_version;
    uint32_t firmware_version;
    uint32_t eeprom_version;
    
    // ===== Timing =====
    uint32_t reset_start_time;
    uint32_t busy_wait_start_time;
};

/*
    COMMAND TYPES AND STRUCTURE
*/
typedef enum {
    CMD_START_SCAN,
    CMD_STOP_SCAN,
    CMD_READ_UID,
    CMD_READ_BLOCK,
    CMD_WRITE_BLOCK,
    CMD_AUTHENTICATE,
    CMD_SLEEP,
    CMD_WAKEUP,
    CMD_RESET
} pn5180_command_type_t;

typedef struct {
    pn5180_command_type_t type;
    union {
        struct {
            uint8_t protocols;
        } start_scan;
        struct {
            pn5180_protocol_t protocol;
            uint32_t timeout_ms;
        } read_uid;
        struct {
            pn5180_card_info_t card;
            uint16_t block;
            uint32_t timeout_ms;
        } read_block;
        struct {
            pn5180_card_info_t card;
            uint16_t block;
            const uint8_t *data;
            uint32_t timeout_ms;
        } write_block;
    } params;
    void *response_buffer;
    size_t response_size;
    SemaphoreHandle_t completion_semaphore;
    pn5180_error_t *error_result;
} pn5180_command_t;

/*
    LOW-LEVEL SPI FUNCTIONS (Internal)
*/
pn5180_error_t pn5180_spi_init(pn5180_dev_t *dev);
pn5180_error_t pn5180_spi_deinit(pn5180_dev_t *dev);
pn5180_error_t pn5180_write_register_internal(pn5180_dev_t *dev, uint8_t reg, uint32_t value);
pn5180_error_t pn5180_read_register_internal(pn5180_dev_t *dev, uint8_t reg, uint32_t *value);
pn5180_error_t pn5180_write_buffer(pn5180_dev_t *dev, const uint8_t *data, size_t length);
pn5180_error_t pn5180_read_buffer(pn5180_dev_t *dev, uint8_t *buffer, size_t *length);
pn5180_error_t pn5180_wait_busy(pn5180_dev_t *dev, uint32_t timeout_ms);
pn5180_error_t pn5180_send_command(pn5180_dev_t *dev, const uint8_t *cmd, size_t cmd_len);

/*
    GPIO AND RESET FUNCTIONS (Internal)
*/
pn5180_error_t pn5180_gpio_init(pn5180_dev_t *dev);
pn5180_error_t pn5180_gpio_deinit(pn5180_dev_t *dev);
pn5180_error_t pn5180_reset_hardware(pn5180_dev_t *dev);
pn5180_error_t pn5180_reset_software(pn5180_dev_t *dev);

/*
    EEPROM AND CONFIGURATION FUNCTIONS (Internal)
*/
pn5180_error_t pn5180_load_eeprom_data(pn5180_dev_t *dev);
pn5180_error_t pn5180_configure_defaults(pn5180_dev_t *dev);
pn5180_error_t pn5180_configure_protocols(pn5180_dev_t *dev);
pn5180_error_t pn5180_check_communication(pn5180_dev_t *dev);

/*
    STATE MACHINE FUNCTIONS (Internal)
*/
void pn5180_state_task(void *arg);
pn5180_error_t pn5180_handle_command(pn5180_dev_t *dev, pn5180_command_t *cmd);
pn5180_error_t pn5180_switch_protocol(pn5180_dev_t *dev, pn5180_protocol_t protocol);
pn5180_error_t pn5180_send_detect_command(pn5180_dev_t *dev, pn5180_protocol_t protocol);

/*
    PROTOCOL-SPECIFIC RESPONSE PROCESSING
*/
pn5180_error_t pn5180_process_iso14443a_response(pn5180_dev_t *dev, uint8_t *response, size_t length, pn5180_card_info_t *card);
pn5180_error_t pn5180_process_iso15693_response(pn5180_dev_t *dev, uint8_t *response, size_t length, pn5180_card_info_t *card);

/*
    UTILITY FUNCTIONS (Internal)
*/
uint32_t pn5180_get_tick_ms(void);
void pn5180_delay_ms(uint32_t ms);
void pn5180_log(pn5180_dev_t *dev, const char *format, ...);
void pn5180_dump_registers(pn5180_dev_t *dev);
void pn5180_dump_buffer(const char *label, const uint8_t *buffer, size_t length);

/*
    CRC CALCULATION FUNCTIONS (Internal)
*/  
uint16_t pn5180_crc_iso14443a(const uint8_t *data, size_t length);
uint16_t pn5180_crc_iso15693(const uint8_t *data, size_t length);

#ifdef __cplusplus
}
#endif

#endif // PN5180_PRIVATE_H