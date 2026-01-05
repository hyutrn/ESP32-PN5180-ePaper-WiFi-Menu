#include "pn5180_private.h"
#include "pn5180_registers.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_timer.h"

static const char *TAG = "PN5180_SPI";

// ==============================================
// SPI TRANSACTION HELPERS
// ==============================================

/**
 * @brief Check BUSY pin state with timeout
 */
static bool pn5180_is_busy_active(pn5180_dev_t *dev) {
    return gpio_get_level(dev->pin_busy) == 1;
}

/**
 * @brief Wait for BUSY pin to go low with timeout
 */
pn5180_error_t pn5180_wait_busy(pn5180_dev_t *dev, uint32_t timeout_ms) {
    if (!dev) {
        return PN5180_ERR_INVALID_ARG;
    }

    uint32_t start_time = pn5180_get_tick_ms();
    
    while (pn5180_is_busy_active(dev)) {
        if (pn5180_get_tick_ms() - start_time > timeout_ms) {
            pn5180_log_internal(dev, 2, "BUSY timeout after %lu ms", timeout_ms);
            return PN5180_ERR_TIMEOUT;
        }
        // Short delay to avoid busy loop
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    return PN5180_OK;
}

/**
 * @brief Execute SPI transaction with error handling
 */
static esp_err_t pn5180_spi_transaction(pn5180_dev_t *dev, spi_transaction_t *trans) {
    esp_err_t err;
    
    // Take SPI mutex
    if (xSemaphoreTake(dev->spi_mutex, pdMS_TO_TICKS(CONFIG_PN5180_SPI_TIMEOUT_MS)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Wait for BUSY pin if necessary
    if (pn5180_wait_busy(dev, CONFIG_PN5180_BUSY_TIMEOUT_MS) != PN5180_OK) {
        xSemaphoreGive(dev->spi_mutex);
        return ESP_ERR_TIMEOUT;
    }
    
    // Execute SPI transaction
    err = spi_device_transmit(dev->spi_device, trans);
    
    // Release SPI mutex
    xSemaphoreGive(dev->spi_mutex);
    
    return err;
}

// ==============================================
// SPI INITIALIZATION
// ==============================================

pn5180_error_t pn5180_spi_init(pn5180_dev_t *dev) {
    esp_err_t err;
    
    if (!dev) {
        return PN5180_ERR_INVALID_ARG;
    }
    
    // SPI bus configuration
    spi_bus_config_t bus_cfg = {
        .miso_io_num = dev->pin_miso,
        .mosi_io_num = dev->pin_mosi,
        .sclk_io_num = dev->pin_sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = CONFIG_PN5180_TX_BUFFER_SIZE + CONFIG_PN5180_RX_BUFFER_SIZE,
        .flags = SPICOMMON_BUSFLAG_MASTER,
        .intr_flags = 0
    };
    
    // Initialize SPI bus
    err = spi_bus_initialize(dev->spi_host, &bus_cfg, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI bus initialization failed: %s", esp_err_to_name(err));
        return PN5180_ERR_SPI;
    }
    
    // SPI device configuration
    spi_device_interface_config_t dev_cfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .mode = CONFIG_PN5180_SPI_MODE,
        .clock_speed_hz = dev->spi_clock_hz,
        .spics_io_num = dev->pin_nss,
        .queue_size = CONFIG_PN5180_SPI_QUEUE_SIZE,
        .flags = SPI_DEVICE_HALFDUPLEX,
        .pre_cb = NULL,
        .post_cb = NULL
    };
    
    // Attach device to SPI bus
    err = spi_bus_add_device(dev->spi_host, &dev_cfg, &dev->spi_device);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI device add failed: %s", esp_err_to_name(err));
        spi_bus_free(dev->spi_host);
        return PN5180_ERR_SPI;
    }
    
    ESP_LOGI(TAG, "SPI initialized: host=%d, clock=%luHz, mode=%d", 
             dev->spi_host, dev->spi_clock_hz, CONFIG_PN5180_SPI_MODE);
    
    return PN5180_OK;
}

pn5180_error_t pn5180_spi_deinit(pn5180_dev_t *dev) {
    if (!dev) {
        return PN5180_ERR_INVALID_ARG;
    }
    
    if (dev->spi_device) {
        spi_bus_remove_device(dev->spi_device);
        dev->spi_device = NULL;
    }
    
    spi_bus_free(dev->spi_host);
    
    ESP_LOGI(TAG, "SPI deinitialized");
    
    return PN5180_OK;
}

// ==============================================
// REGISTER ACCESS FUNCTIONS
// ==============================================

/**
 * @brief Write 32-bit value to PN5180 register
 * Format: [1-bit write flag | 7-bit address] [32-bit data MSB first]
 */
pn5180_error_t pn5180_write_register_internal(pn5180_dev_t *dev, uint8_t reg, uint32_t value) {
    if (!dev || !dev->spi_device) {
        return PN5180_ERR_NOT_INIT;
    }
    
    // Prepare transmit buffer (5 bytes: command + 32-bit value)
    uint8_t tx_buffer[5];
    tx_buffer[0] = PN5180_SPI_WRITE_MASK | (reg & 0x7F);  // MSB=1 for write
    tx_buffer[1] = (value >> 24) & 0xFF;  // MSB first
    tx_buffer[2] = (value >> 16) & 0xFF;
    tx_buffer[3] = (value >> 8) & 0xFF;
    tx_buffer[4] = value & 0xFF;
    
    // SPI transaction
    spi_transaction_t trans = {
        .length = sizeof(tx_buffer) * 8,  // Length in bits
        .tx_buffer = tx_buffer,
        .rx_buffer = NULL,
        .user = NULL
    };
    
    esp_err_t err = pn5180_spi_transaction(dev, &trans);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI write register failed: %s (reg=0x%02X)", esp_err_to_name(err), reg);
        return PN5180_ERR_SPI;
    }
    
#if CONFIG_PN5180_DEBUG_LEVEL >= 3
    ESP_LOGD(TAG, "Write reg 0x%02X = 0x%08lX", reg, value);
#endif
    
    return PN5180_OK;
}

/**
 * @brief Read 32-bit value from PN5180 register
 * Format: [1-bit read flag | 7-bit address] then read 32-bit data
 */
pn5180_error_t pn5180_read_register_internal(pn5180_dev_t *dev, uint8_t reg, uint32_t *value) {
    if (!dev || !dev->spi_device || !value) {
        return PN5180_ERR_INVALID_ARG;
    }
    
    // Prepare transmit buffer (1 byte command) and receive buffer (4 bytes)
    uint8_t tx_buffer[1] = {reg & 0x7F};  // MSB=0 for read
    uint8_t rx_buffer[4] = {0};
    
    // SPI transaction - first send read command
    spi_transaction_t trans_cmd = {
        .length = sizeof(tx_buffer) * 8,
        .tx_buffer = tx_buffer,
        .rx_buffer = NULL,
        .user = NULL
    };
    
    esp_err_t err = pn5180_spi_transaction(dev, &trans_cmd);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI read register command failed: %s (reg=0x%02X)", esp_err_to_name(err), reg);
        return PN5180_ERR_SPI;
    }
    
    // Then read 32-bit data
    spi_transaction_t trans_data = {
        .length = sizeof(rx_buffer) * 8,
        .tx_buffer = NULL,
        .rx_buffer = rx_buffer,
        .user = NULL
    };
    
    err = pn5180_spi_transaction(dev, &trans_data);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SPI read register data failed: %s (reg=0x%02X)", esp_err_to_name(err), reg);
        return PN5180_ERR_SPI;
    }
    
    // Combine bytes (MSB first)
    *value = ((uint32_t)rx_buffer[0] << 24) |
             ((uint32_t)rx_buffer[1] << 16) |
             ((uint32_t)rx_buffer[2] << 8)  |
             ((uint32_t)rx_buffer[3]);
    
#if CONFIG_PN5180_DEBUG_LEVEL >= 3
    ESP_LOGD(TAG, "Read reg 0x%02X = 0x%08lX", reg, *value);
#endif
    
    return PN5180_OK;
}

/**
 * @brief Read multiple registers sequentially
 */
pn5180_error_t pn5180_read_registers(pn5180_dev_t *dev, uint8_t start_reg, uint32_t *values, size_t count) {
    if (!dev || !values || count == 0) {
        return PN5180_ERR_INVALID_ARG;
    }
    
    for (size_t i = 0; i < count; i++) {
        pn5180_error_t err = pn5180_read_register_internal(dev, start_reg + i, &values[i]);
        if (err != PN5180_OK) {
            return err;
        }
    }
    
    return PN5180_OK;
}

// ==============================================
// BUFFER ACCESS FUNCTIONS
// ==============================================

/**
 * @brief Write data to TX buffer (FIFO)
 * Note: TX_DATA register auto-increments
 */
pn5180_error_t pn5180_write_buffer(pn5180_dev_t *dev, const uint8_t *data, size_t length) {
    if (!dev || !dev->spi_device || !data || length == 0) {
        return PN5180_ERR_INVALID_ARG;
    }
    
    if (length > CONFIG_PN5180_TX_BUFFER_SIZE) {
        ESP_LOGE(TAG, "TX buffer overflow: %zu > %d", length, CONFIG_PN5180_TX_BUFFER_SIZE);
        return PN5180_ERR_BUFFER;
    }
    
    // Set TX pointer to beginning of buffer
    pn5180_error_t err = pn5180_write_register_internal(dev, PN5180_REG_TX_DATA, 0);
    if (err != PN5180_OK) {
        return err;
    }
    
    // Write data in chunks (SPI may have max transfer size)
    size_t offset = 0;
    while (offset < length) {
        size_t chunk_size = length - offset;
        if (chunk_size > 64) {  // Reasonable chunk size
            chunk_size = 64;
        }
        
        // Prepare SPI transaction
        spi_transaction_t trans = {
            .length = chunk_size * 8,
            .tx_buffer = data + offset,
            .rx_buffer = NULL,
            .user = NULL
        };
        
        esp_err_t spi_err = pn5180_spi_transaction(dev, &trans);
        if (spi_err != ESP_OK) {
            ESP_LOGE(TAG, "SPI write buffer failed at offset %zu: %s", offset, esp_err_to_name(spi_err));
            return PN5180_ERR_SPI;
        }
        
        offset += chunk_size;
    }
    
    // Store TX length for later use
    dev->tx_length = length;
    
#if CONFIG_PN5180_DEBUG_LEVEL >= 3
    ESP_LOGD(TAG, "Wrote %zu bytes to TX buffer", length);
    if (CONFIG_PN5180_DEBUG_LEVEL >= 4) {
        pn5180_dump_buffer("TX", data, length);
    }
#endif
    
    return PN5180_OK;
}

/**
 * @brief Read data from RX buffer (FIFO)
 * Note: RX_DATA register auto-increments
 */
pn5180_error_t pn5180_read_buffer(pn5180_dev_t *dev, uint8_t *buffer, size_t *length) {
    if (!dev || !dev->spi_device || !buffer || !length) {
        return PN5180_ERR_INVALID_ARG;
    }
    
    // Set RX pointer to beginning of buffer
    pn5180_error_t err = pn5180_write_register_internal(dev, PN5180_REG_RX_DATA, 0);
    if (err != PN5180_OK) {
        return err;
    }
    
    // Check how much data is available (optional - you might know the expected length)
    size_t bytes_to_read = *length;
    if (bytes_to_read > CONFIG_PN5180_RX_BUFFER_SIZE) {
        bytes_to_read = CONFIG_PN5180_RX_BUFFER_SIZE;
    }
    
    // Read data in chunks
    size_t offset = 0;
    while (offset < bytes_to_read) {
        size_t chunk_size = bytes_to_read - offset;
        if (chunk_size > 64) {  // Reasonable chunk size
            chunk_size = 64;
        }
        
        // Prepare SPI transaction
        spi_transaction_t trans = {
            .length = chunk_size * 8,
            .tx_buffer = NULL,
            .rx_buffer = buffer + offset,
            .user = NULL
        };
        
        esp_err_t spi_err = pn5180_spi_transaction(dev, &trans);
        if (spi_err != ESP_OK) {
            ESP_LOGE(TAG, "SPI read buffer failed at offset %zu: %s", offset, esp_err_to_name(spi_err));
            return PN5180_ERR_SPI;
        }
        
        offset += chunk_size;
    }
    
    *length = offset;
    dev->rx_length = offset;
    
#if CONFIG_PN5180_DEBUG_LEVEL >= 3
    ESP_LOGD(TAG, "Read %zu bytes from RX buffer", offset);
    if (CONFIG_PN5180_DEBUG_LEVEL >= 4) {
        pn5180_dump_buffer("RX", buffer, offset);
    }
#endif
    
    return PN5180_OK;
}

/**
 * @brief Write data to TX buffer and set TX length
 */
pn5180_error_t pn5180_write_tx_buffer_with_length(pn5180_dev_t *dev, const uint8_t *data, size_t length) {
    pn5180_error_t err;
    
    // Write data to buffer
    err = pn5180_write_buffer(dev, data, length);
    if (err != PN5180_OK) {
        return err;
    }
    
    // Set TX length registers
    err = pn5180_write_register_internal(dev, PN5180_REG_TX_LENGTH_LSB, length & 0xFF);
    if (err != PN5180_OK) {
        return err;
    }
    
    err = pn5180_write_register_internal(dev, PN5180_REG_TX_LENGTH_MSB, (length >> 8) & 0xFF);
    if (err != PN5180_OK) {
        return err;
    }
    
    return PN5180_OK;
}

// ==============================================
// COMMAND EXECUTION FUNCTIONS
// ==============================================

/**
 * @brief Send command and wait for response
 */
pn5180_error_t pn5180_send_command(pn5180_dev_t *dev, const uint8_t *cmd, size_t cmd_len) {
    if (!dev || !cmd || cmd_len == 0) {
        return PN5180_ERR_INVALID_ARG;
    }
    
    // Write command to TX buffer
    pn5180_error_t err = pn5180_write_tx_buffer_with_length(dev, cmd, cmd_len);
    if (err != PN5180_OK) {
        ESP_LOGE(TAG, "Failed to write command to buffer");
        return err;
    }
    
    // Clear TX done IRQ status
    err = pn5180_write_register_internal(dev, PN5180_REG_IRQ_STATUS, PN5180_IRQ_STATUS_TX_DONE);
    if (err != PN5180_OK) {
        ESP_LOGE(TAG, "Failed to clear TX IRQ status");
        return err;
    }
    
    // Enable TX done interrupt
    err = pn5180_write_register_internal(dev, PN5180_REG_IRQ_ENABLE, PN5180_IRQ_ENABLE_TX_DONE);
    if (err != PN5180_OK) {
        ESP_LOGE(TAG, "Failed to enable TX IRQ");
        return err;
    }
    
    // Start transmission by enabling RF field (if not already on)
    uint32_t rf_control;
    err = pn5180_read_register_internal(dev, PN5180_REG_RF_CONTROL, &rf_control);
    if (err != PN5180_OK) {
        return err;
    }
    
    if (!(rf_control & PN5180_RF_CONTROL_FIELD_ON)) {
        rf_control |= PN5180_RF_CONTROL_FIELD_ON;
        err = pn5180_write_register_internal(dev, PN5180_REG_RF_CONTROL, rf_control);
        if (err != PN5180_OK) {
            return err;
        }
    }
    
    ESP_LOGD(TAG, "Command sent (%zu bytes), waiting for TX done...", cmd_len);
    
    // Wait for TX completion
    uint32_t start_time = pn5180_get_tick_ms();
    uint32_t irq_status = 0;
    
    while (1) {
        // Check timeout
        if (pn5180_get_tick_ms() - start_time > CONFIG_PN5180_SPI_TIMEOUT_MS) {
            ESP_LOGE(TAG, "TX timeout");
            return PN5180_ERR_TIMEOUT;
        }
        
        // Read IRQ status
        err = pn5180_read_register_internal(dev, PN5180_REG_IRQ_STATUS, &irq_status);
        if (err != PN5180_OK) {
            return err;
        }
        
        if (irq_status & PN5180_IRQ_STATUS_TX_DONE) {
            break;  // TX completed
        }
        
        if (irq_status & PN5180_IRQ_STATUS_TX_ERROR) {
            ESP_LOGE(TAG, "TX error occurred");
            return PN5180_ERR_SPI;
        }
        
        // Small delay to avoid busy loop
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    // Clear IRQ status
    err = pn5180_write_register_internal(dev, PN5180_REG_IRQ_STATUS, 
                                        PN5180_IRQ_STATUS_TX_DONE | PN5180_IRQ_STATUS_TX_ERROR);
    if (err != PN5180_OK) {
        ESP_LOGE(TAG, "Failed to clear IRQ status after TX");
        return err;
    }
    
    ESP_LOGD(TAG, "TX completed successfully");
    
    return PN5180_OK;
}

/**
 * @brief Wait for RX data with timeout
 */
pn5180_error_t pn5180_wait_for_rx(pn5180_dev_t *dev, uint32_t timeout_ms, size_t *rx_length) {
    if (!dev || !rx_length) {
        return PN5180_ERR_INVALID_ARG;
    }
    
    // Enable RX done interrupt
    pn5180_error_t err = pn5180_write_register_internal(dev, PN5180_REG_IRQ_ENABLE, 
                                                       PN5180_IRQ_ENABLE_RX_DONE | PN5180_IRQ_ENABLE_RX_ERROR);
    if (err != PN5180_OK) {
        return err;
    }
    
    // Clear RX IRQ status
    err = pn5180_write_register_internal(dev, PN5180_REG_IRQ_STATUS, 
                                        PN5180_IRQ_STATUS_RX_DONE | PN5180_IRQ_STATUS_RX_ERROR);
    if (err != PN5180_OK) {
        return err;
    }
    
    ESP_LOGD(TAG, "Waiting for RX (timeout=%lums)...", timeout_ms);
    
    // Wait for RX completion or error
    uint32_t start_time = pn5180_get_tick_ms();
    uint32_t irq_status = 0;
    
    while (1) {
        // Check timeout
        if (pn5180_get_tick_ms() - start_time > timeout_ms) {
            ESP_LOGE(TAG, "RX timeout");
            return PN5180_ERR_TIMEOUT;
        }
        
        // Read IRQ status
        err = pn5180_read_register_internal(dev, PN5180_REG_IRQ_STATUS, &irq_status);
        if (err != PN5180_OK) {
            return err;
        }
        
        if (irq_status & PN5180_IRQ_STATUS_RX_DONE) {
            // Success - get RX length from FIFO status or known protocol length
            *rx_length = dev->rx_length;  // You might need to read actual length
            break;
        }
        
        if (irq_status & PN5180_IRQ_STATUS_RX_ERROR) {
            ESP_LOGE(TAG, "RX error occurred");
            
            // Read error status for debugging
            uint32_t error_status;
            pn5180_read_register_internal(dev, PN5180_REG_ERROR_STATUS, &error_status);
            ESP_LOGE(TAG, "Error status: 0x%08lX", error_status);
            
            return PN5180_ERR_PROTOCOL;
        }
        
        // Small delay
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    // Clear IRQ status
    err = pn5180_write_register_internal(dev, PN5180_REG_IRQ_STATUS, 
                                        PN5180_IRQ_STATUS_RX_DONE | PN5180_IRQ_STATUS_RX_ERROR);
    if (err != PN5180_OK) {
        return err;
    }
    
    ESP_LOGD(TAG, "RX completed, received %zu bytes", *rx_length);
    
    return PN5180_OK;
}

/**
 * @brief Send command and receive response
 */
pn5180_error_t pn5180_transceive(pn5180_dev_t *dev, 
                                 const uint8_t *tx_data, size_t tx_len,
                                 uint8_t *rx_buffer, size_t *rx_len, 
                                 uint32_t timeout_ms) {
    pn5180_error_t err;
    size_t actual_rx_len = 0;
    
    // Send command
    err = pn5180_send_command(dev, tx_data, tx_len);
    if (err != PN5180_OK) {
        return err;
    }
    
    // Wait for response
    err = pn5180_wait_for_rx(dev, timeout_ms, &actual_rx_len);
    if (err != PN5180_OK) {
        return err;
    }
    
    // Read response if buffer provided
    if (rx_buffer && rx_len) {
        size_t read_len = actual_rx_len;
        if (read_len > *rx_len) {
            read_len = *rx_len;
        }
        
        err = pn5180_read_buffer(dev, rx_buffer, &read_len);
        if (err != PN5180_OK) {
            return err;
        }
        
        *rx_len = read_len;
    }
    
    return PN5180_OK;
}

// ==============================================
// UTILITY FUNCTIONS
// ==============================================

/**
 * @brief Dump buffer contents for debugging
 */
void pn5180_dump_buffer(const char *label, const uint8_t *buffer, size_t length) {
    if (CONFIG_PN5180_DEBUG_LEVEL < 3 || !buffer || length == 0) {
        return;
    }
    
    char line[80];
    char *ptr = line;
    
    ptr += sprintf(ptr, "%s [%zu]: ", label, length);
    
    for (size_t i = 0; i < length && i < 16; i++) {
        ptr += sprintf(ptr, "%02X ", buffer[i]);
    }
    
    if (length > 16) {
        ptr += sprintf(ptr, "...");
    }
    
    ESP_LOGD(TAG, "%s", line);
}

/**
 * @brief Dump all important registers for debugging
 */
void pn5180_dump_registers(pn5180_dev_t *dev) {
    if (CONFIG_PN5180_DEBUG_LEVEL < 2 || !dev) {
        return;
    }
    
    uint32_t values[32];
    const char *reg_names[] = {
        "SYSTEM_CONFIG", "IRQ_ENABLE", "IRQ_STATUS", "ERROR_STATUS",
        "STATUS", "RF_CONTROL", "RF_STATUS", "RX_CONF1",
        "RX_CONF3", "TX_CONF1", "TX_DRV_A", "TX_DRV_15693",
        "TX_DATA", "RX_DATA", "FIFO_CTRL", "FIFO_STAT",
        "TX_TIMER", "RX_TIMER", "PWR_DOWN", "ANT_CTRL",
        "CRC_CFG", "EEPROM_VER", "PROD_VER", "FW_VER"
    };
    
    uint8_t regs[] = {
        PN5180_REG_SYSTEM_CONFIG, PN5180_REG_IRQ_ENABLE, PN5180_REG_IRQ_STATUS,
        PN5180_REG_ERROR_STATUS, PN5180_REG_STATUS, PN5180_REG_RF_CONTROL,
        PN5180_REG_RF_STATUS, PN5180_REG_RX_CONF1, PN5180_REG_RX_CONF3,
        PN5180_REG_TX_CONF1, PN5180_REG_ISO14443A_TX_DRIVER,
        PN5180_REG_ISO15693_TX_DRIVER, PN5180_REG_TX_DATA, PN5180_REG_RX_DATA,
        PN5180_REG_FIFO_CONTROL, PN5180_REG_FIFO_STATUS,
        PN5180_REG_TX_TIMER_CONFIG, PN5180_REG_RX_TIMER_CONFIG,
        PN5180_REG_POWER_DOWN, PN5180_REG_ANTENNA_CTRL, PN5180_REG_CRC_CONFIG,
        0x14, 0x10, 0x12  // EEPROM addresses
    };
    
    ESP_LOGI(TAG, "=== PN5180 REGISTER DUMP ===");
    
    for (size_t i = 0; i < sizeof(regs) / sizeof(regs[0]); i++) {
        if (pn5180_read_register_internal(dev, regs[i], &values[i]) == PN5180_OK) {
            ESP_LOGI(TAG, "%-15s (0x%02X) = 0x%08lX", 
                    reg_names[i], regs[i], values[i]);
        } else {
            ESP_LOGW(TAG, "%-15s (0x%02X) = READ FAILED", 
                    reg_names[i], regs[i]);
        }
    }
    
    ESP_LOGI(TAG, "=== END REGISTER DUMP ===");
}

/**
 * @brief Test SPI communication by reading/writing test register
 */
pn5180_error_t pn5180_test_spi(pn5180_dev_t *dev) {
    if (!dev) {
        return PN5180_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Testing SPI communication...");
    
    // Try to read a known register (SYSTEM_CONFIG)
    uint32_t value;
    pn5180_error_t err = pn5180_read_register_internal(dev, PN5180_REG_SYSTEM_CONFIG, &value);
    if (err != PN5180_OK) {
        ESP_LOGE(TAG, "SPI test failed: cannot read SYSTEM_CONFIG");
        return err;
    }
    
    ESP_LOGI(TAG, "SYSTEM_CONFIG = 0x%08lX", value);
    
    // Try to write and read back a test value to a register
    // Use RF_CONTROL register (we'll write a safe value)
    uint32_t original_value;
    err = pn5180_read_register_internal(dev, PN5180_REG_RF_CONTROL, &original_value);
    if (err != PN5180_OK) {
        ESP_LOGE(TAG, "SPI test failed: cannot read RF_CONTROL");
        return err;
    }
    
    // Write test value (turn RF field OFF for safety)
    uint32_t test_value = original_value & ~PN5180_RF_CONTROL_FIELD_ON;
    err = pn5180_write_register_internal(dev, PN5180_REG_RF_CONTROL, test_value);
    if (err != PN5180_OK) {
        ESP_LOGE(TAG, "SPI test failed: cannot write RF_CONTROL");
        return err;
    }
    
    // Read back
    uint32_t readback_value;
    err = pn5180_read_register_internal(dev, PN5180_REG_RF_CONTROL, &readback_value);
    if (err != PN5180_OK) {
        ESP_LOGE(TAG, "SPI test failed: cannot read back RF_CONTROL");
        return err;
    }
    
    // Restore original value
    pn5180_write_register_internal(dev, PN5180_REG_RF_CONTROL, original_value);
    
    if ((readback_value & 0x8F) != (test_value & 0x8F)) {  // Mask out reserved bits
        ESP_LOGE(TAG, "SPI test failed: write/read mismatch. Wrote 0x%08lX, read 0x%08lX", 
                test_value, readback_value);
        return PN5180_ERR_SPI;
    }
    
    ESP_LOGI(TAG, "SPI communication test PASSED");
    return PN5180_OK;
}

// ==============================================
// POWER MANAGEMENT FUNCTIONS
// ==============================================

pn5180_error_t pn5180_enter_sleep_mode(pn5180_dev_t *dev) {
    if (!dev) {
        return PN5180_ERR_INVALID_ARG;
    }
    
    // Ensure RF field is off
    uint32_t rf_control;
    pn5180_error_t err = pn5180_read_register_internal(dev, PN5180_REG_RF_CONTROL, &rf_control);
    if (err != PN5180_OK) {
        return err;
    }
    
    if (rf_control & PN5180_RF_CONTROL_FIELD_ON) {
        rf_control &= ~PN5180_RF_CONTROL_FIELD_ON;
        err = pn5180_write_register_internal(dev, PN5180_REG_RF_CONTROL, rf_control);
        if (err != PN5180_OK) {
            return err;
        }
    }
    
    // Enter power down mode
    err = pn5180_write_register_internal(dev, PN5180_REG_SYSTEM_CONFIG, 
                                        PN5180_SYSTEM_CONFIG_POWER_DOWN);
    if (err != PN5180_OK) {
        return err;
    }
    
    dev->state = DEVICE_STATE_SLEEP;
    ESP_LOGI(TAG, "Entered sleep mode");
    
    return PN5180_OK;
}

pn5180_error_t pn5180_wake_from_sleep(pn5180_dev_t *dev) {
    if (!dev) {
        return PN5180_ERR_INVALID_ARG;
    }
    
    // Clear power down bit
    pn5180_error_t err = pn5180_write_register_internal(dev, PN5180_REG_SYSTEM_CONFIG, 0);
    if (err != PN5180_OK) {
        return err;
    }
    
    // Small delay for wake-up
    vTaskDelay(pdMS_TO_TICKS(10));
    
    // Re-initialize important registers
    err = pn5180_configure_defaults(dev);
    if (err != PN5180_OK) {
        return err;
    }
    
    dev->state = DEVICE_STATE_IDLE;
    ESP_LOGI(TAG, "Woke from sleep mode");
    
    return PN5180_OK;
}