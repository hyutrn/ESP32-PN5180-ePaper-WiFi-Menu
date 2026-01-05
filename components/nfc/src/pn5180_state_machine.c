#include "pn5180_private.h"
#include "pn5180_registers.h"
#include "pn5180_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "string.h"

static const char *TAG = "PN5180_STATE";

// ==============================================
// PROTOCOL DETECTION COMMANDS
// ==============================================

static const uint8_t detect_cmd_iso14443a[] = {0x26};  // REQA
static const uint8_t detect_cmd_iso14443b[] = {0x05, 0x00};  // REQB (AFI=0x00)
static const uint8_t detect_cmd_iso15693[] = {0x26, 0x01};  // Inventory (1 slot)

// ==============================================
// STATE MACHINE CORE
// ==============================================

void pn5180_state_task(void *arg) {
    pn5180_dev_t *dev = (pn5180_dev_t*)arg;
    pn5180_command_t cmd;
    TickType_t last_wake_time = xTaskGetTickCount();
    
    if (!dev) {
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "State machine task started");
    
    while (1) {
        // Check for incoming commands (non-blocking)
        if (xQueueReceive(dev->command_queue, &cmd, 0) == pdTRUE) {
            pn5180_error_t result = pn5180_handle_command(dev, &cmd);
            
            // If command expects a response, signal completion
            if (cmd.completion_semaphore) {
                if (cmd.error_result) {
                    *cmd.error_result = result;
                }
                xSemaphoreGive(cmd.completion_semaphore);
            }
        }
        
        // Handle current state
        switch (dev->state) {
            case DEVICE_STATE_UNINITIALIZED:
                // Wait for initialization
                break;
                
            case DEVICE_STATE_RESETTING:
                pn5180_state_resetting(dev);
                break;
                
            case DEVICE_STATE_IDLE:
                pn5180_state_idle(dev);
                break;
                
            case DEVICE_STATE_CONFIGURING:
                pn5180_state_configuring(dev);
                break;
                
            case DEVICE_STATE_SCANNING:
                pn5180_state_scanning(dev);
                break;
                
            case DEVICE_STATE_TRANSMITTING:
                pn5180_state_transmitting(dev);
                break;
                
            case DEVICE_STATE_RECEIVING:
                pn5180_state_receiving(dev);
                break;
                
            case DEVICE_STATE_PROCESSING:
                pn5180_state_processing(dev);
                break;
                
            case DEVICE_STATE_ERROR:
                pn5180_state_error(dev);
                break;
                
            case DEVICE_STATE_SLEEP:
                pn5180_state_sleep(dev);
                break;
                
            case DEVICE_STATE_WAKING_UP:
                pn5180_state_waking_up(dev);
                break;
        }
        
        // Small delay to prevent busy loop
        vTaskDelayUntil(&last_wake_time, pdMS_TO_TICKS(1));
    }
}

// ==============================================
// COMMAND HANDLING
// ==============================================

pn5180_error_t pn5180_handle_command(pn5180_dev_t *dev, pn5180_command_t *cmd) {
    if (!dev || !cmd) {
        return PN5180_ERR_INVALID_ARG;
    }
    
    ESP_LOGD(TAG, "Handling command type: %d", cmd->type);
    
    switch (cmd->type) {
        case CMD_START_SCAN:
            return pn5180_cmd_start_scan(dev, cmd);
            
        case CMD_STOP_SCAN:
            return pn5180_cmd_stop_scan(dev, cmd);
            
        case CMD_READ_UID:
            return pn5180_cmd_read_uid(dev, cmd);
            
        case CMD_READ_BLOCK:
            return pn5180_cmd_read_block(dev, cmd);
            
        case CMD_WRITE_BLOCK:
            return pn5180_cmd_write_block(dev, cmd);
            
        case CMD_AUTHENTICATE:
            return pn5180_cmd_authenticate(dev, cmd);
            
        case CMD_SLEEP:
            return pn5180_cmd_sleep(dev, cmd);
            
        case CMD_WAKEUP:
            return pn5180_cmd_wakeup(dev, cmd);
            
        case CMD_RESET:
            return pn5180_cmd_reset(dev, cmd);
            
        default:
            ESP_LOGW(TAG, "Unknown command type: %d", cmd->type);
            return PN5180_ERR_UNSUPPORTED;
    }
}

// ==============================================
// STATE HANDLERS
// ==============================================

void pn5180_state_resetting(pn5180_dev_t *dev) {
    if (pn5180_get_tick_ms() - dev->state_timestamp < 50) {
        return; // Wait 50ms after reset
    }
    
    // Configure device after reset
    pn5180_error_t err = pn5180_configure_defaults(dev);
    if (err != PN5180_OK) {
        ESP_LOGE(TAG, "Failed to configure after reset: %d", err);
        dev->state = DEVICE_STATE_ERROR;
        return;
    }
    
    // Re-enable scanning if it was active
    if (dev->scanning_enabled) {
        dev->state = DEVICE_STATE_CONFIGURING;
    } else {
        dev->state = DEVICE_STATE_IDLE;
    }
}

void pn5180_state_idle(pn5180_dev_t *dev) {
    // Nothing to do in idle state
    // Just wait for commands
}

void pn5180_state_configuring(pn5180_dev_t *dev) {
    // Configure for scanning
    pn5180_error_t err = pn5180_configure_for_scanning(dev);
    if (err != PN5180_OK) {
        ESP_LOGE(TAG, "Failed to configure for scanning: %d", err);
        dev->state = DEVICE_STATE_ERROR;
        return;
    }
    
    dev->state = DEVICE_STATE_SCANNING;
    dev->current_protocol_index = 0;
    dev->last_scan_time = pn5180_get_tick_ms();
}

void pn5180_state_scanning(pn5180_dev_t *dev) {
    if (!dev->scanning_enabled) {
        dev->state = DEVICE_STATE_IDLE;
        return;
    }
    
    // Calculate time since last scan
    uint32_t current_time = pn5180_get_tick_ms();
    if (current_time - dev->last_scan_time < CONFIG_PN5180_SCAN_CYCLE_DELAY_MS) {
        return; // Wait between scans
    }
    
    // Find next enabled protocol
    uint8_t start_index = dev->current_protocol_index;
    while (1) {
        if (dev->enabled_protocols & (1 << dev->current_protocol_index)) {
            break; // Found enabled protocol
        }
        
        dev->current_protocol_index++;
        if (dev->current_protocol_index >= PN5180_PROTOCOL_COUNT) {
            dev->current_protocol_index = 0;
        }
        
        if (dev->current_protocol_index == start_index) {
            // No enabled protocols
            dev->state = DEVICE_STATE_IDLE;
            return;
        }
    }
    
    // Switch to protocol
    pn5180_error_t err = pn5180_switch_protocol(dev, dev->current_protocol_index);
    if (err != PN5180_OK) {
        ESP_LOGE(TAG, "Failed to switch protocol: %d", err);
        dev->current_protocol_index++; // Try next protocol
        dev->last_scan_time = current_time;
        return;
    }
    
    // Send detection command
    err = pn5180_send_detect_command(dev, dev->current_protocol_index);
    if (err != PN5180_OK) {
        ESP_LOGE(TAG, "Failed to send detect command: %d", err);
        dev->current_protocol_index++;
        dev->last_scan_time = current_time;
        return;
    }
    
    dev->state = DEVICE_STATE_TRANSMITTING;
    dev->state_timestamp = current_time;
    dev->total_scans++;
}

void pn5180_state_transmitting(pn5180_dev_t *dev) {
    // Check for TX completion
    uint32_t irq_status;
    pn5180_error_t err = pn5180_read_register_internal(dev, PN5180_REG_IRQ_STATUS, &irq_status);
    if (err != PN5180_OK) {
        ESP_LOGE(TAG, "Failed to read IRQ status: %d", err);
        dev->state = DEVICE_STATE_ERROR;
        return;
    }
    
    if (irq_status & PN5180_IRQ_STATUS_TX_DONE) {
        // Clear TX done IRQ
        pn5180_write_register_internal(dev, PN5180_REG_IRQ_STATUS, PN5180_IRQ_STATUS_TX_DONE);
        
        // Switch to receive mode
        dev->state = DEVICE_STATE_RECEIVING;
        dev->state_timestamp = pn5180_get_tick_ms();
        
        // Set RX timeout based on protocol
        uint32_t timeout_ms = 0;
        switch (dev->current_protocol_index) {
            case PN5180_PROTOCOL_ISO14443A:
                timeout_ms = CONFIG_PN5180_DETECT_TIMEOUT_14443A_MS;
                break;
            case PN5180_PROTOCOL_ISO14443B:
                timeout_ms = CONFIG_PN5180_DETECT_TIMEOUT_14443B_MS;
                break;
            case PN5180_PROTOCOL_ISO15693:
                timeout_ms = CONFIG_PN5180_DETECT_TIMEOUT_15693_MS;
                break;
            default:
                timeout_ms = 5;
        }
        
        dev->busy_wait_start_time = pn5180_get_tick_ms() + timeout_ms;
    }
    else if (irq_status & PN5180_IRQ_STATUS_TX_ERROR) {
        ESP_LOGE(TAG, "TX error detected");
        pn5180_write_register_internal(dev, PN5180_REG_IRQ_STATUS, PN5180_IRQ_STATUS_TX_ERROR);
        dev->current_protocol_index++;
        dev->state = DEVICE_STATE_SCANNING;
    }
    else if (pn5180_get_tick_ms() - dev->state_timestamp > 100) {
        // TX timeout
        ESP_LOGW(TAG, "TX timeout");
        dev->current_protocol_index++;
        dev->state = DEVICE_STATE_SCANNING;
    }
}

void pn5180_state_receiving(pn5180_dev_t *dev) {
    // Check for RX completion
    uint32_t irq_status;
    pn5180_error_t err = pn5180_read_register_internal(dev, PN5180_REG_IRQ_STATUS, &irq_status);
    if (err != PN5180_OK) {
        ESP_LOGE(TAG, "Failed to read IRQ status: %d", err);
        dev->state = DEVICE_STATE_ERROR;
        return;
    }
    
    if (irq_status & PN5180_IRQ_STATUS_RX_DONE) {
        // Clear RX done IRQ
        pn5180_write_register_internal(dev, PN5180_REG_IRQ_STATUS, PN5180_IRQ_STATUS_RX_DONE);
        
        // Process the received data
        dev->state = DEVICE_STATE_PROCESSING;
    }
    else if (irq_status & PN5180_IRQ_STATUS_RX_ERROR) {
        ESP_LOGE(TAG, "RX error detected");
        pn5180_write_register_internal(dev, PN5180_REG_IRQ_STATUS, PN5180_IRQ_STATUS_RX_ERROR);
        dev->current_protocol_index++;
        dev->state = DEVICE_STATE_SCANNING;
    }
    else if (pn5180_get_tick_ms() > dev->busy_wait_start_time) {
        // RX timeout - no card detected
        dev->current_protocol_index++;
        dev->state = DEVICE_STATE_SCANNING;
        dev->last_scan_time = pn5180_get_tick_ms();
    }
}

void pn5180_state_processing(pn5180_dev_t *dev) {
    // Read RX buffer
    size_t rx_length = CONFIG_PN5180_RX_BUFFER_SIZE;
    uint8_t rx_buffer[CONFIG_PN5180_RX_BUFFER_SIZE];
    
    pn5180_error_t err = pn5180_read_buffer(dev, rx_buffer, &rx_length);
    if (err != PN5180_OK) {
        ESP_LOGE(TAG, "Failed to read RX buffer: %d", err);
        dev->state = DEVICE_STATE_SCANNING;
        return;
    }
    
    // Process based on protocol
    pn5180_card_info_t card_info = {0};
    card_info.protocol = dev->current_protocol_index;
    card_info.timestamp = pn5180_get_tick_ms();
    
    switch (dev->current_protocol_index) {
        case PN5180_PROTOCOL_ISO14443A:
            err = pn5180_process_iso14443a_response(dev, rx_buffer, rx_length, &card_info);
            break;
        case PN5180_PROTOCOL_ISO15693:
            err = pn5180_process_iso15693_response(dev, rx_buffer, rx_length, &card_info);
            break;
        default:
            ESP_LOGW(TAG, "Unsupported protocol for processing: %d", dev->current_protocol_index);
            err = PN5180_ERR_UNSUPPORTED;
    }
    
    if (err == PN5180_OK) {
        // Card detected successfully
        dev->cards_detected++;
        
        // Get RSSI
        uint32_t rf_status;
        if (pn5180_read_register_internal(dev, PN5180_REG_RF_STATUS, &rf_status) == PN5180_OK) {
            card_info.rssi = rf_status & PN5180_RF_STATUS_RSSI_MASK;
        }
        
        // Call callback if set
        if (dev->card_callback) {
            dev->card_callback(&card_info, dev->callback_user_data);
        }
    }
    
    // Move to next protocol and continue scanning
    dev->current_protocol_index++;
    dev->state = DEVICE_STATE_SCANNING;
    dev->last_scan_time = pn5180_get_tick_ms();
}

void pn5180_state_error(pn5180_dev_t *dev) {
    dev->error_count++;
    
    // Try to recover after some time
    if (pn5180_get_tick_ms() - dev->state_timestamp > 1000) {
        ESP_LOGI(TAG, "Attempting error recovery");
        
        // Soft reset
        pn5180_write_register_internal(dev, PN5180_REG_SYSTEM_CONFIG, PN5180_SYSTEM_CONFIG_RESET);
        vTaskDelay(pdMS_TO_TICKS(10));
        
        // Reconfigure
        pn5180_configure_defaults(dev);
        
        if (dev->scanning_enabled) {
            dev->state = DEVICE_STATE_CONFIGURING;
        } else {
            dev->state = DEVICE_STATE_IDLE;
        }
        
        dev->state_timestamp = pn5180_get_tick_ms();
    }
}

void pn5180_state_sleep(pn5180_dev_t *dev) {
    // Nothing to do in sleep state
    // Device will be woken by command
}

void pn5180_state_waking_up(pn5180_dev_t *dev) {
    // Wait for device to stabilize
    if (pn5180_get_tick_ms() - dev->state_timestamp < 50) {
        return;
    }
    
    // Reconfigure device
    pn5180_configure_defaults(dev);
    
    dev->state = DEVICE_STATE_IDLE;
}

// ==============================================
// COMMAND IMPLEMENTATIONS
// ==============================================

pn5180_error_t pn5180_cmd_start_scan(pn5180_dev_t *dev, pn5180_command_t *cmd) {
    if (!dev || !cmd) {
        return PN5180_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(dev->device_mutex, portMAX_DELAY);
    
    if (dev->scanning_enabled) {
        xSemaphoreGive(dev->device_mutex);
        return PN5180_OK; // Already scanning
    }
    
    dev->enabled_protocols = cmd->params.start_scan.protocols;
    dev->scanning_enabled = true;
    dev->current_protocol_index = 0;
    
    // Start from configuring state
    dev->state = DEVICE_STATE_CONFIGURING;
    dev->state_timestamp = pn5180_get_tick_ms();
    
    xSemaphoreGive(dev->device_mutex);
    
    ESP_LOGI(TAG, "Scanning started for protocols: 0x%02X", dev->enabled_protocols);
    return PN5180_OK;
}

pn5180_error_t pn5180_cmd_stop_scan(pn5180_dev_t *dev, pn5180_command_t *cmd) {
    if (!dev) {
        return PN5180_ERR_INVALID_ARG;
    }
    
    xSemaphoreTake(dev->device_mutex, portMAX_DELAY);
    
    if (!dev->scanning_enabled) {
        xSemaphoreGive(dev->device_mutex);
        return PN5180_OK; // Already stopped
    }
    
    dev->scanning_enabled = false;
    
    // Turn off RF field
    uint32_t rf_control;
    if (pn5180_read_register_internal(dev, PN5180_REG_RF_CONTROL, &rf_control) == PN5180_OK) {
        rf_control &= ~PN5180_RF_CONTROL_FIELD_ON;
        pn5180_write_register_internal(dev, PN5180_REG_RF_CONTROL, rf_control);
    }
    
    dev->state = DEVICE_STATE_IDLE;
    
    xSemaphoreGive(dev->device_mutex);
    
    ESP_LOGI(TAG, "Scanning stopped");
    return PN5180_OK;
}

pn5180_error_t pn5180_cmd_read_uid(pn5180_dev_t *dev, pn5180_command_t *cmd) {
    // This is a synchronous command - we'll handle it directly
    if (!dev || !cmd || !cmd->response_buffer) {
        return PN5180_ERR_INVALID_ARG;
    }
    
    pn5180_card_info_t *card_info = (pn5180_card_info_t*)cmd->response_buffer;
    
    // Switch to requested protocol
    pn5180_error_t err = pn5180_switch_protocol(dev, cmd->params.read_uid.protocol);
    if (err != PN5180_OK) {
        return err;
    }
    
    // Send detection command
    err = pn5180_send_detect_command(dev, cmd->params.read_uid.protocol);
    if (err != PN5180_OK) {
        return err;
    }
    
    // Wait for response
    uint32_t start_time = pn5180_get_tick_ms();
    while (1) {
        uint32_t irq_status;
        err = pn5180_read_register_internal(dev, PN5180_REG_IRQ_STATUS, &irq_status);
        if (err != PN5180_OK) {
            return err;
        }
        
        if (irq_status & PN5180_IRQ_STATUS_RX_DONE) {
            pn5180_write_register_internal(dev, PN5180_REG_IRQ_STATUS, PN5180_IRQ_STATUS_RX_DONE);
            break;
        }
        
        if (irq_status & PN5180_IRQ_STATUS_RX_ERROR) {
            pn5180_write_register_internal(dev, PN5180_REG_IRQ_STATUS, PN5180_IRQ_STATUS_RX_ERROR);
            return PN5180_ERR_PROTOCOL;
        }
        
        if (pn5180_get_tick_ms() - start_time > cmd->params.read_uid.timeout_ms) {
            return PN5180_ERR_TIMEOUT;
        }
        
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    // Read response
    size_t rx_length = CONFIG_PN5180_RX_BUFFER_SIZE;
    uint8_t rx_buffer[CONFIG_PN5180_RX_BUFFER_SIZE];
    err = pn5180_read_buffer(dev, rx_buffer, &rx_length);
    if (err != PN5180_OK) {
        return err;
    }
    
    // Process response
    card_info->protocol = cmd->params.read_uid.protocol;
    card_info->timestamp = pn5180_get_tick_ms();
    
    switch (cmd->params.read_uid.protocol) {
        case PN5180_PROTOCOL_ISO14443A:
            err = pn5180_process_iso14443a_response(dev, rx_buffer, rx_length, card_info);
            break;
        case PN5180_PROTOCOL_ISO15693:
            err = pn5180_process_iso15693_response(dev, rx_buffer, rx_length, card_info);
            break;
        default:
            err = PN5180_ERR_UNSUPPORTED;
    }
    
    return err;
}

pn5180_error_t pn5180_cmd_read_block(pn5180_dev_t *dev, pn5180_command_t *cmd) {
    // Simplified - just return error for now
    ESP_LOGW(TAG, "CMD_READ_BLOCK not implemented yet");
    return PN5180_ERR_UNSUPPORTED;
}

pn5180_error_t pn5180_cmd_write_block(pn5180_dev_t *dev, pn5180_command_t *cmd) {
    ESP_LOGW(TAG, "CMD_WRITE_BLOCK not implemented yet");
    return PN5180_ERR_UNSUPPORTED;
}

pn5180_error_t pn5180_cmd_authenticate(pn5180_dev_t *dev, pn5180_command_t *cmd) {
    ESP_LOGW(TAG, "CMD_AUTHENTICATE not implemented yet");
    return PN5180_ERR_UNSUPPORTED;
}

pn5180_error_t pn5180_cmd_sleep(pn5180_dev_t *dev, pn5180_command_t *cmd) {
    if (!dev) {
        return PN5180_ERR_INVALID_ARG;
    }
    
    pn5180_error_t err = pn5180_enter_sleep_mode(dev);
    if (err == PN5180_OK) {
        dev->state = DEVICE_STATE_SLEEP;
    }
    
    return err;
}

pn5180_error_t pn5180_cmd_wakeup(pn5180_dev_t *dev, pn5180_command_t *cmd) {
    if (!dev) {
        return PN5180_ERR_INVALID_ARG;
    }
    
    pn5180_error_t err = pn5180_wake_from_sleep(dev);
    if (err == PN5180_OK) {
        dev->state = DEVICE_STATE_WAKING_UP;
        dev->state_timestamp = pn5180_get_tick_ms();
    }
    
    return err;
}

pn5180_error_t pn5180_cmd_reset(pn5180_dev_t *dev, pn5180_command_t *cmd) {
    if (!dev) {
        return PN5180_ERR_INVALID_ARG;
    }
    
    // Perform soft reset
    pn5180_error_t err = pn5180_write_register_internal(dev, PN5180_REG_SYSTEM_CONFIG, PN5180_SYSTEM_CONFIG_RESET);
    if (err != PN5180_OK) {
        return err;
    }
    
    dev->state = DEVICE_STATE_RESETTING;
    dev->state_timestamp = pn5180_get_tick_ms();
    
    return PN5180_OK;
}

// ==============================================
// HELPER FUNCTIONS
// ==============================================

pn5180_error_t pn5180_configure_for_scanning(pn5180_dev_t *dev) {
    if (!dev) {
        return PN5180_ERR_INVALID_ARG;
    }
    
    // Enable RF field
    uint32_t rf_control;
    pn5180_error_t err = pn5180_read_register_internal(dev, PN5180_REG_RF_CONTROL, &rf_control);
    if (err != PN5180_OK) {
        return err;
    }
    
    rf_control |= PN5180_RF_CONTROL_FIELD_ON;
    err = pn5180_write_register_internal(dev, PN5180_REG_RF_CONTROL, rf_control);
    if (err != PN5180_OK) {
        return err;
    }
    
    // Clear any pending IRQs
    pn5180_write_register_internal(dev, PN5180_REG_IRQ_STATUS, 0xFF);
    
    // Enable necessary interrupts
    uint32_t irq_enable = PN5180_IRQ_ENABLE_TX_DONE | 
                         PN5180_IRQ_ENABLE_RX_DONE |
                         PN5180_IRQ_ENABLE_RX_ERROR |
                         PN5180_IRQ_ENABLE_TX_ERROR;
    
    err = pn5180_write_register_internal(dev, PN5180_REG_IRQ_ENABLE, irq_enable);
    if (err != PN5180_OK) {
        return err;
    }
    
    return PN5180_OK;
}

pn5180_error_t pn5180_switch_protocol(pn5180_dev_t *dev, pn5180_protocol_t protocol) {
    if (!dev || protocol >= PN5180_PROTOCOL_COUNT) {
        return PN5180_ERR_INVALID_ARG;
    }
    
    pn5180_error_t err;
    
    switch (protocol) {
        case PN5180_PROTOCOL_ISO14443A:
            // Configure for ISO14443A (106kbps, 100% ASK)
            err = pn5180_write_register_internal(dev, PN5180_REG_TX_CONF1, 
                (PN5180_TX_BITRATE_106KBPS << PN5180_TX_CONF1_BITRATE_SEL_POS) |
                (PN5180_TX_MOD_100_PERCENT_ASK << PN5180_TX_CONF1_MOD_TYPE_POS));
            
            if (err != PN5180_OK) return err;
            
            // Set TX power
            err = pn5180_write_register_internal(dev, PN5180_REG_ISO14443A_TX_DRIVER, 
                dev->rf_config.tx_power & PN5180_TX_DRIVER_STRENGTH_MASK);
            break;
            
        case PN5180_PROTOCOL_ISO15693:
            // Configure for ISO15693 (26.48kbps, 10% ASK)
            err = pn5180_write_register_internal(dev, PN5180_REG_TX_CONF1, 
                (0x01 << PN5180_TX_CONF1_BITRATE_SEL_POS) |  // 26.48kbps
                (PN5180_TX_MOD_10_PERCENT_ASK << PN5180_TX_CONF1_MOD_TYPE_POS));
            
            if (err != PN5180_OK) return err;
            
            // Set TX power
            err = pn5180_write_register_internal(dev, PN5180_REG_ISO15693_TX_DRIVER, 
                dev->rf_config.tx_power & PN5180_TX_DRIVER_STRENGTH_MASK);
            break;
            
        default:
            ESP_LOGW(TAG, "Protocol %d not implemented for switching", protocol);
            return PN5180_ERR_UNSUPPORTED;
    }
    
    if (err != PN5180_OK) {
        return err;
    }
    
    // Update receiver gain
    err = pn5180_write_register_internal(dev, PN5180_REG_RX_CONF1, 
        (dev->rf_config.rx_gain & PN5180_RX_CONF1_GAIN_SEL_MASK) |
        (PN5180_RX_BW_1_8MHZ << PN5180_RX_CONF1_BW_SEL_POS));
    
    if (err != PN5180_OK) {
        return err;
    }
    
    // Update detection threshold
    err = pn5180_write_register_internal(dev, PN5180_REG_RX_CONF3,
        (dev->rf_config.iq_threshold << PN5180_RX_CONF3_IQ_DET_THR_POS) |
        (dev->rf_config.modulation_depth << PN5180_RX_CONF3_MIN_MOD_DEPTH_POS));
    
    return err;
}

pn5180_error_t pn5180_send_detect_command(pn5180_dev_t *dev, pn5180_protocol_t protocol) {
    if (!dev) {
        return PN5180_ERR_INVALID_ARG;
    }
    
    const uint8_t *cmd = NULL;
    size_t cmd_len = 0;
    
    switch (protocol) {
        case PN5180_PROTOCOL_ISO14443A:
            cmd = detect_cmd_iso14443a;
            cmd_len = sizeof(detect_cmd_iso14443a);
            break;
        case PN5180_PROTOCOL_ISO15693:
            cmd = detect_cmd_iso15693;
            cmd_len = sizeof(detect_cmd_iso15693);
            break;
        default:
            ESP_LOGW(TAG, "No detect command for protocol %d", protocol);
            return PN5180_ERR_UNSUPPORTED;
    }
    
    if (!cmd || cmd_len == 0) {
        return PN5180_ERR_INVALID_ARG;
    }
    
    // Write command to TX buffer
    pn5180_error_t err = pn5180_write_tx_buffer_with_length(dev, cmd, cmd_len);
    if (err != PN5180_OK) {
        return err;
    }
    
    // Clear TX IRQ
    err = pn5180_write_register_internal(dev, PN5180_REG_IRQ_STATUS, PN5180_IRQ_STATUS_TX_DONE);
    if (err != PN5180_OK) {
        return err;
    }
    
    // Start transmission (RF field should already be on)
    ESP_LOGD(TAG, "Sending detect command for protocol %d", protocol);
    
    return PN5180_OK;
}

// ==============================================
// PUBLIC API WRAPPERS
// ==============================================

pn5180_error_t pn5180_start_scanning(pn5180_dev_t *dev, uint8_t protocols, 
                                    pn5180_card_callback_t callback, void *user_data) {
    if (!dev) {
        return PN5180_ERR_INVALID_ARG;
    }
    
    // Set callback
    pn5180_set_card_callback(dev, callback, user_data);
    
    // Prepare command
    pn5180_command_t cmd = {
        .type = CMD_START_SCAN,
        .params.start_scan.protocols = protocols,
        .completion_semaphore = NULL,
        .error_result = NULL
    };
    
    // Send command to queue
    if (xQueueSend(dev->command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        return PN5180_ERR_TIMEOUT;
    }
    
    return PN5180_OK;
}

pn5180_error_t pn5180_stop_scanning(pn5180_dev_t *dev) {
    if (!dev) {
        return PN5180_ERR_INVALID_ARG;
    }
    
    pn5180_command_t cmd = {
        .type = CMD_STOP_SCAN,
        .completion_semaphore = NULL,
        .error_result = NULL
    };
    
    if (xQueueSend(dev->command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        return PN5180_ERR_TIMEOUT;
    }
    
    return PN5180_OK;
}