#include "pn5180.h"
#include "pn5180_private.h"
#include "pn5180_config.h"


static const char *TAG = CONFIG_PN5180_LOG_TAG;

// ==============================================
// PRIVATE HELPER FUNCTIONS
// ==============================================

static uint32_t pn5180_get_tick_ms(void) {
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void pn5180_delay_ms(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static void pn5180_log_internal(pn5180_dev_t *dev, int level, const char *format, ...) {
    if (!dev || level > CONFIG_PN5180_DEBUG_LEVEL) {
        return;
    }
    
    va_list args;
    va_start(args, format);
    
    if (dev->log_callback) {
        char buffer[256];
        vsnprintf(buffer, sizeof(buffer), format, args);
        dev->log_callback(buffer, dev->callback_user_data);
    } else {
        switch (level) {
            case 1:
                ESP_LOGE(TAG, format, args);
                break;
            case 2:
                ESP_LOGI(TAG, format, args);
                break;
            case 3:
                ESP_LOGD(TAG, format, args);
                break;
        }
    }
    
    va_end(args);
}

// ==============================================
// PUBLIC API: INITIALIZATION
// ==============================================

pn5180_dev_t* pn5180_init(const pn5180_pin_config_t *pin_cfg, pn5180_rf_config_t *rf_cfg) {
    if (!pin_cfg) {
        ESP_LOGE(TAG, "Pin configuration is NULL");
        return NULL;
    }
    
    // Allocate device structure
    pn5180_dev_t *dev = (pn5180_dev_t*)calloc(1, sizeof(pn5180_dev_t));
    if (!dev) {
        ESP_LOGE(TAG, "Failed to allocate device structure");
        return NULL;
    }
    
    // Store pin configuration
    dev->pin_miso = pin_cfg->miso_pin;
    dev->pin_mosi = pin_cfg->mosi_pin;
    dev->pin_sclk = pin_cfg->sclk_pin;
    dev->pin_nss = pin_cfg->nss_pin;
    dev->pin_busy = pin_cfg->busy_pin;
    dev->pin_rst = pin_cfg->rst_pin;
    dev->pin_irq = pin_cfg->irq_pin;
    
    // Set SPI parameters
    dev->spi_host = CONFIG_PN5180_SPI_HOST;
    dev->spi_clock_hz = CONFIG_PN5180_SPI_CLOCK_HZ;
    
    // Set default state
    dev->state = DEVICE_STATE_UNINITIALIZED;
    
    // Initialize RF configuration
    if (rf_cfg) {
        memcpy(&dev->rf_config, rf_cfg, sizeof(pn5180_rf_config_t));
    } else {
        // Default RF configuration for maximum range
        dev->rf_config.rx_gain = CONFIG_PN5180_RX_GAIN_DEFAULT;
        dev->rf_config.tx_power = CONFIG_PN5180_TX_POWER_DEFAULT;
        dev->rf_config.modulation_depth = 0x02;
        dev->rf_config.iq_threshold = CONFIG_PN5180_IQ_THRESHOLD_DEFAULT;
        dev->rf_config.antenna_tuning = CONFIG_PN5180_ANTENNA_TUNING_DEFAULT;
        dev->rf_config.crc_enabled = 1;
        dev->rf_config.auto_rf_control = 1;
        dev->rf_config.lpcd_enabled = CONFIG_PN5180_LPCD_ENABLED;
    }
    
    // Create mutexes
    dev->spi_mutex = xSemaphoreCreateMutex();
    if (!dev->spi_mutex) {
        ESP_LOGE(TAG, "Failed to create SPI mutex");
        free(dev);
        return NULL;
    }
    
    dev->device_mutex = xSemaphoreCreateMutex();
    if (!dev->device_mutex) {
        ESP_LOGE(TAG, "Failed to create device mutex");
        vSemaphoreDelete(dev->spi_mutex);
        free(dev);
        return NULL;
    }
    
    // Create command queue
    dev->command_queue = xQueueCreate(CONFIG_PN5180_COMMAND_QUEUE_SIZE, sizeof(pn5180_command_t));
    if (!dev->command_queue) {
        ESP_LOGE(TAG, "Failed to create command queue");
        vSemaphoreDelete(dev->spi_mutex);
        vSemaphoreDelete(dev->device_mutex);
        free(dev);
        return NULL;
    }
    
    // Initialize GPIO pins
    if (pn5180_gpio_init(dev) != PN5180_OK) {
        ESP_LOGE(TAG, "Failed to initialize GPIO");
        vSemaphoreDelete(dev->spi_mutex);
        vSemaphoreDelete(dev->device_mutex);
        vQueueDelete(dev->command_queue);
        free(dev);
        return NULL;
    }
    
    // Initialize SPI
    if (pn5180_spi_init(dev) != PN5180_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI");
        pn5180_gpio_deinit(dev);
        vSemaphoreDelete(dev->spi_mutex);
        vSemaphoreDelete(dev->device_mutex);
        vQueueDelete(dev->command_queue);
        free(dev);
        return NULL;
    }
    
    // Perform hardware reset
    ESP_LOGI(TAG, "Performing hardware reset...");
    if (pn5180_reset_hardware(dev) != PN5180_OK) {
        ESP_LOGE(TAG, "Hardware reset failed");
        pn5180_spi_deinit(dev);
        pn5180_gpio_deinit(dev);
        vSemaphoreDelete(dev->spi_mutex);
        vSemaphoreDelete(dev->device_mutex);
        vQueueDelete(dev->command_queue);
        free(dev);
        return NULL;
    }
    
    // Check communication by reading a known register
    ESP_LOGI(TAG, "Checking communication...");
    if (pn5180_check_communication(dev) != PN5180_OK) {
        ESP_LOGE(TAG, "Communication check failed");
        pn5180_spi_deinit(dev);
        pn5180_gpio_deinit(dev);
        vSemaphoreDelete(dev->spi_mutex);
        vSemaphoreDelete(dev->device_mutex);
        vQueueDelete(dev->command_queue);
        free(dev);
        return NULL;
    }
    
    // Load EEPROM data (product version, firmware, etc.)
    if (pn5180_load_eeprom_data(dev) != PN5180_OK) {
        ESP_LOGW(TAG, "Failed to load EEPROM data, using defaults");
    }
    
    // Configure default registers
    if (pn5180_configure_defaults(dev) != PN5180_OK) {
        ESP_LOGE(TAG, "Failed to configure defaults");
        pn5180_spi_deinit(dev);
        pn5180_gpio_deinit(dev);
        vSemaphoreDelete(dev->spi_mutex);
        vSemaphoreDelete(dev->device_mutex);
        vQueueDelete(dev->command_queue);
        free(dev);
        return NULL;
    }
    
    // Configure protocol-specific settings
    if (pn5180_configure_protocols(dev) != PN5180_OK) {
        ESP_LOGE(TAG, "Failed to configure protocols");
        pn5180_spi_deinit(dev);
        pn5180_gpio_deinit(dev);
        vSemaphoreDelete(dev->spi_mutex);
        vSemaphoreDelete(dev->device_mutex);
        vQueueDelete(dev->command_queue);
        free(dev);
        return NULL;
    }
    
    // Create state machine task
    BaseType_t result = xTaskCreate(
        pn5180_state_task,
        "pn5180_state",
        4096,  // Stack size
        dev,
        5,     // Priority
        &dev->state_task_handle
    );
    
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create state task");
        pn5180_spi_deinit(dev);
        pn5180_gpio_deinit(dev);
        vSemaphoreDelete(dev->spi_mutex);
        vSemaphoreDelete(dev->device_mutex);
        vQueueDelete(dev->command_queue);
        free(dev);
        return NULL;
    }
    
    dev->state = DEVICE_STATE_IDLE;
    ESP_LOGI(TAG, "PN5180 initialized successfully");
    ESP_LOGI(TAG, "Product Version: 0x%08lX", dev->product_version);
    ESP_LOGI(TAG, "Firmware Version: 0x%08lX", dev->firmware_version);
    
    return dev;
}

void pn5180_deinit(pn5180_dev_t *dev) {
    if (!dev) {
        return;
    }
    
    ESP_LOGI(TAG, "Deinitializing PN5180...");
    
    // Stop scanning if active
    dev->scanning_enabled = false;
    
    // Delete state task
    if (dev->state_task_handle) {
        vTaskDelete(dev->state_task_handle);
        dev->state_task_handle = NULL;
    }
    
    // Power down the device
    pn5180_sleep(dev);
    
    // Deinitialize SPI
    pn5180_spi_deinit(dev);
    
    // Deinitialize GPIO
    pn5180_gpio_deinit(dev);
    
    // Delete synchronization primitives
    if (dev->spi_mutex) {
        vSemaphoreDelete(dev->spi_mutex);
    }
    
    if (dev->device_mutex) {
        vSemaphoreDelete(dev->device_mutex);
    }
    
    if (dev->command_queue) {
        vQueueDelete(dev->command_queue);
    }
    
    // Free device structure
    free(dev);
    
    ESP_LOGI(TAG, "PN5180 deinitialized");
}

// ==============================================
// PUBLIC API: STATUS AND INFO
// ==============================================

pn5180_error_t pn5180_get_version(pn5180_dev_t *dev, uint32_t *product, uint32_t *firmware) {
    if (!dev || !product || !firmware) {
        return PN5180_ERR_INVALID_ARG;
    }
    
    if (dev->state == DEVICE_STATE_UNINITIALIZED) {
        return PN5180_ERR_NOT_INIT;
    }
    
    *product = dev->product_version;
    *firmware = dev->firmware_version;
    
    return PN5180_OK;
}

bool pn5180_is_busy(pn5180_dev_t *dev) {
    if (!dev) {
        return true;
    }
    
    xSemaphoreTake(dev->device_mutex, portMAX_DELAY);
    bool busy = (dev->state != DEVICE_STATE_IDLE);
    xSemaphoreGive(dev->device_mutex);
    
    return busy;
}

const char* pn5180_error_to_string(pn5180_error_t error) {
    switch (error) {
        case PN5180_OK: return "OK";
        case PN5180_ERR_INVALID_ARG: return "Invalid argument";
        case PN5180_ERR_TIMEOUT: return "Timeout";
        case PN5180_ERR_CRC: return "CRC error";
        case PN5180_ERR_AUTH: return "Authentication error";
        case PN5180_ERR_PROTOCOL: return "Protocol error";
        case PN5180_ERR_BUFFER: return "Buffer error";
        case PN5180_ERR_SPI: return "SPI communication error";
        case PN5180_ERR_NO_TAG: return "No tag detected";
        case PN5180_ERR_MULTIPLE_TAGS: return "Multiple tags detected";
        case PN5180_ERR_HARDWARE: return "Hardware error";
        case PN5180_ERR_NOT_INIT: return "Device not initialized";
        case PN5180_ERR_BUSY: return "Device busy";
        case PN5180_ERR_RF_FIELD: return "RF field error";
        case PN5180_ERR_EEPROM: return "EEPROM error";
        case PN5180_ERR_UNSUPPORTED: return "Unsupported operation";
        default: return "Unknown error";
    }
}

// ==============================================
// PUBLIC API: CALLBACK SETUP
// ==============================================

void pn5180_set_card_callback(pn5180_dev_t *dev, pn5180_card_callback_t callback, void *user_data) {
    if (!dev) return;
    
    xSemaphoreTake(dev->device_mutex, portMAX_DELAY);
    dev->card_callback = callback;
    dev->callback_user_data = user_data;
    xSemaphoreGive(dev->device_mutex);
}

void pn5180_set_error_callback(pn5180_dev_t *dev, pn5180_error_callback_t callback, void *user_data) {
    if (!dev) return;
    
    xSemaphoreTake(dev->device_mutex, portMAX_DELAY);
    dev->error_callback = callback;
    dev->callback_user_data = user_data;
    xSemaphoreGive(dev->device_mutex);
}

void pn5180_set_log_callback(pn5180_dev_t *dev, pn5180_log_callback_t callback, void *user_data) {
    if (!dev) return;
    
    xSemaphoreTake(dev->device_mutex, portMAX_DELAY);
    dev->log_callback = callback;
    dev->callback_user_data = user_data;
    xSemaphoreGive(dev->device_mutex);
}