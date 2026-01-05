#include "epaper.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include <string.h>

static const char *TAG = "EPAPER";

// Lệnh điều khiển SSD1680
#define EPD_CMD_DRIVER_OUTPUT_CONTROL 0x01
#define EPD_CMD_BOOSTER_SOFT_START 0x0C
#define EPD_CMD_GATE_SCAN_START_POS 0x0F
#define EPD_CMD_DEEP_SLEEP_MODE 0x10
#define EPD_CMD_DATA_ENTRY_MODE 0x11
#define EPD_CMD_SW_RESET 0x12
#define EPD_CMD_TEMP_CONTROL 0x18
#define EPD_CMD_MASTER_ACTIVATION 0x20
#define EPD_CMD_DISPLAY_UPDATE_CONTROL_1 0x21
#define EPD_CMD_DISPLAY_UPDATE_CONTROL_2 0x22
#define EPD_CMD_WRITE_RAM_BW 0x24
#define EPD_CMD_WRITE_RAM_RED 0x26
#define EPD_CMD_VCOM_REGISTER 0x2C
#define EPD_CMD_BORDER_WAVEFORM_CONTROL 0x3C
#define EPD_CMD_RAM_X_ADDR_START_END 0x44
#define EPD_CMD_RAM_Y_ADDR_START_END 0x45
#define EPD_CMD_RAM_X_ADDR_COUNTER 0x4E
#define EPD_CMD_RAM_Y_ADDR_COUNTER 0x4F
#define EPD_CMD_SET_DUMMY_LINE_PERIOD 0x3A
#define EPD_CMD_SET_GATE_TIME 0x3B

// Cấu trúc dữ liệu nội bộ
typedef struct {
    spi_device_handle_t spi;
    int busy_pin;
    int rst_pin;
    int dc_pin;
    int cs_pin;
    epd_rotation_t rotation;
    bool swap_red_black;
    uint16_t width;
    uint16_t height;
    uint8_t *frame_buffer_black;
    uint8_t *frame_buffer_red;
} epd_dev_t;

// Hàm trợ giúp
static void epd_write_command(epd_handle_t handle, uint8_t cmd) {
    epd_dev_t *dev = (epd_dev_t *)handle;
    
    gpio_set_level(dev->dc_pin, 0);  // DC = 0: command
    spi_transaction_t trans = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    gpio_set_level(dev->cs_pin, 0);
    spi_device_transmit(dev->spi, &trans);
    gpio_set_level(dev->cs_pin, 1);
}

static void epd_write_data(epd_handle_t handle, const uint8_t *data, size_t len) {
    epd_dev_t *dev = (epd_dev_t *)handle;
    
    gpio_set_level(dev->dc_pin, 1);  // DC = 1: data
    spi_transaction_t trans = {
        .length = len * 8,
        .tx_buffer = data,
    };
    gpio_set_level(dev->cs_pin, 0);
    spi_device_transmit(dev->spi, &trans);
    gpio_set_level(dev->cs_pin, 1);
}

static void epd_write_data_byte(epd_handle_t handle, uint8_t data) {
    epd_write_data(handle, &data, 1);
}

static void epd_wait_busy(epd_handle_t handle) {
    epd_dev_t *dev = (epd_dev_t *)handle;
    
    ESP_LOGD(TAG, "Waiting for ePaper BUSY...");
    while (gpio_get_level(dev->busy_pin) == 1) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    ESP_LOGD(TAG, "ePaper ready");
}

// Khởi tạo ePaper
epd_handle_t epd_init(const epd_config_t *config) {
    epd_dev_t *dev = calloc(1, sizeof(epd_dev_t));
    if (!dev) {
        ESP_LOGE(TAG, "Failed to allocate memory");
        return NULL;
    }
    
    // Lưu cấu hình
    dev->busy_pin = config->busy_pin;
    dev->rst_pin = config->rst_pin;
    dev->dc_pin = config->dc_pin;
    dev->cs_pin = config->cs_pin;
    dev->rotation = config->rotation;
    dev->swap_red_black = config->swap_red_black;
    
    // Cấu hình GPIO
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << dev->rst_pin) | (1ULL << dev->dc_pin) | (1ULL << dev->cs_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    io_conf.pin_bit_mask = (1ULL << dev->busy_pin);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);
    
    // Thiết lập mức logic ban đầu
    gpio_set_level(dev->cs_pin, 1);
    gpio_set_level(dev->dc_pin, 0);
    
    // Cấu hình SPI
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = config->mosi_pin,
        .miso_io_num = -1,  // ePaper không có MISO
        .sclk_io_num = config->sck_pin,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    
    esp_err_t ret = spi_bus_initialize(config->spi_host, &bus_cfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %d", ret);
        free(dev);
        return NULL;
    }
    
    spi_device_interface_config_t dev_cfg = {
        .clock_speed_hz = 20000000,  // 20MHz
        .mode = 0,  // SPI mode 0
        .spics_io_num = -1,  // CS điều khiển thủ công
        .queue_size = 1,
        .flags = SPI_DEVICE_HALFDUPLEX,
    };
    
    ret = spi_bus_add_device(config->spi_host, &dev_cfg, &dev->spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device: %d", ret);
        free(dev);
        return NULL;
    }
    
    // Tính toán kích thước dựa trên chiều xoay
    if (config->rotation == EPD_ROTATE_0 || config->rotation == EPD_ROTATE_180) {
        dev->width = EPD_WIDTH;
        dev->height = EPD_HEIGHT;
    } else {
        dev->width = EPD_HEIGHT;
        dev->height = EPD_WIDTH;
    }
    
    // Cấp phát buffer (mỗi buffer cần width * height / 8 bytes)
    size_t buffer_size = dev->width * dev->height / 8;
    dev->frame_buffer_black = calloc(1, buffer_size);
    dev->frame_buffer_red = calloc(1, buffer_size);
    
    if (!dev->frame_buffer_black || !dev->frame_buffer_red) {
        ESP_LOGE(TAG, "Failed to allocate frame buffers");
        free(dev->frame_buffer_black);
        free(dev->frame_buffer_red);
        free(dev);
        return NULL;
    }
    
    // Khởi tạo phần cứng
    epd_reset((epd_handle_t)dev);
    
    // Gửi lệnh khởi tạo
    epd_write_command(dev, EPD_CMD_SW_RESET);
    epd_wait_busy(dev);
    
    // Cấu hình driver
    epd_write_command(dev, EPD_CMD_DRIVER_OUTPUT_CONTROL);
    epd_write_data_byte(dev, (EPD_HEIGHT - 1) & 0xFF);
    epd_write_data_byte(dev, ((EPD_HEIGHT - 1) >> 8) & 0xFF);
    epd_write_data_byte(dev, 0x00);  // GD = 0; SM = 0; TB = 0;
    
    epd_write_command(dev, EPD_CMD_BOOSTER_SOFT_START);
    epd_write_data_byte(dev, 0x17);  // Phase 1: 10ms
    epd_write_data_byte(dev, 0x17);  // Phase 2: 10ms
    epd_write_data_byte(dev, 0x17);  // Phase 3: 10ms
    
    epd_write_command(dev, EPD_CMD_DATA_ENTRY_MODE);
    epd_write_data_byte(dev, 0x03);  // X increment; Y increment
    
    // Đặt vùng hiển thị
    epd_write_command(dev, EPD_CMD_RAM_X_ADDR_START_END);
    epd_write_data_byte(dev, 0);  // X start
    epd_write_data_byte(dev, (EPD_WIDTH / 8) - 1);  // X end
    
    epd_write_command(dev, EPD_CMD_RAM_Y_ADDR_START_END);
    epd_write_data_byte(dev, 0);  // Y start
    epd_write_data_byte(dev, (EPD_HEIGHT - 1) & 0xFF);  // Y end low
    epd_write_data_byte(dev, ((EPD_HEIGHT - 1) >> 8) & 0xFF);  // Y end high
    
    epd_write_command(dev, EPD_CMD_BORDER_WAVEFORM_CONTROL);
    epd_write_data_byte(dev, 0x05);
    
    epd_write_command(dev, EPD_CMD_TEMP_CONTROL);
    epd_write_data_byte(dev, 0x80);  // Internal temperature sensor
    
    epd_write_command(dev, EPD_CMD_DISPLAY_UPDATE_CONTROL_1);
    epd_write_data_byte(dev, 0x00);
    epd_write_data_byte(dev, 0x80);
    
    epd_write_command(dev, EPD_CMD_VCOM_REGISTER);
    epd_write_data_byte(dev, 0x9C);  // VCOM 7C
    
    ESP_LOGI(TAG, "ePaper initialized (Rotation: %d, Size: %dx%d)", 
             config->rotation, dev->width, dev->height);
    
    return (epd_handle_t)dev;
}

// Reset ePaper
void epd_reset(epd_handle_t handle) {
    epd_dev_t *dev = (epd_dev_t *)handle;
    
    gpio_set_level(dev->rst_pin, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(dev->rst_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}

// Xoá màn hình với màu chỉ định
void epd_clear(epd_handle_t handle, epd_color_t color) {
    epd_dev_t *dev = (epd_dev_t *)handle;
    size_t buffer_size = dev->width * dev->height / 8;
    
    uint8_t fill_value = 0x00;
    if (color == EPD_COLOR_WHITE) {
        fill_value = 0xFF;  // Tất cả pixel trắng
    } else if (color == EPD_COLOR_BLACK) {
        fill_value = 0x00;  // Tất cả pixel đen
    } else if (color == EPD_COLOR_RED) {
        // Đỏ: đen=trắng, đỏ=đen
        memset(dev->frame_buffer_black, 0xFF, buffer_size);  // Tất cả trắng
        memset(dev->frame_buffer_red, 0x00, buffer_size);    // Tất cả đỏ
        return;
    }
    
    memset(dev->frame_buffer_black, fill_value, buffer_size);
    memset(dev->frame_buffer_red, 0xFF, buffer_size);  // Không có màu đỏ
}

// Hiển thị frame buffer
void epd_display_frame(epd_handle_t handle, const uint8_t *black_buffer, 
                       const uint8_t *red_buffer) {
    epd_dev_t *dev = (epd_dev_t *)handle;
    
    // Đặt địa chỉ RAM về 0
    epd_write_command(handle, EPD_CMD_RAM_X_ADDR_COUNTER);
    epd_write_data_byte(handle, 0);
    epd_write_command(handle, EPD_CMD_RAM_Y_ADDR_COUNTER);
    epd_write_data_byte(handle, 0);
    epd_write_data_byte(handle, 0);
    
    // Ghi dữ liệu đen
    epd_write_command(handle, EPD_CMD_WRITE_RAM_BW);
    if (black_buffer) {
        size_t data_size = EPD_WIDTH * EPD_HEIGHT / 8;
        // Cần xử lý rotation ở đây
        if (dev->rotation == EPD_ROTATE_0) {
            epd_write_data(handle, black_buffer, data_size);
        } else {
            // Xử lý xoay ảnh (cần implement)
            // Tạm thời dùng buffer trực tiếp
            epd_write_data(handle, black_buffer, data_size);
        }
    } else {
        size_t data_size = EPD_WIDTH * EPD_HEIGHT / 8;
        uint8_t *empty_buffer = calloc(1, data_size);
        if (empty_buffer) {
            memset(empty_buffer, 0xFF, data_size);  // Trắng
            epd_write_data(handle, empty_buffer, data_size);
            free(empty_buffer);
        }
    }
    
    // Ghi dữ liệu đỏ
    epd_write_command(handle, EPD_CMD_WRITE_RAM_RED);
    if (red_buffer) {
        size_t data_size = EPD_WIDTH * EPD_HEIGHT / 8;
        epd_write_data(handle, red_buffer, data_size);
    } else {
        size_t data_size = EPD_WIDTH * EPD_HEIGHT / 8;
        uint8_t *empty_buffer = calloc(1, data_size);
        if (empty_buffer) {
            memset(empty_buffer, 0xFF, data_size);  // Không có đỏ
            epd_write_data(handle, empty_buffer, data_size);
            free(empty_buffer);
        }
    }
    
    // Kích hoạt hiển thị
    epd_write_command(handle, EPD_CMD_DISPLAY_UPDATE_CONTROL_2);
    epd_write_data_byte(handle, 0xC7);  // Display Update Sequence
    
    epd_write_command(handle, EPD_CMD_MASTER_ACTIVATION);
    epd_wait_busy(handle);
    
    ESP_LOGI(TAG, "Display updated");
}

// Vẽ pixel
void epd_draw_pixel(epd_handle_t handle, uint16_t x, uint16_t y, epd_color_t color) {
    epd_dev_t *dev = (epd_dev_t *)handle;
    
    // Xử lý rotation
    uint16_t orig_x = x, orig_y = y;
    switch (dev->rotation) {
        case EPD_ROTATE_90:
            x = dev->height - 1 - orig_y;
            y = orig_x;
            break;
        case EPD_ROTATE_180:
            x = dev->width - 1 - orig_x;
            y = dev->height - 1 - orig_y;
            break;
        case EPD_ROTATE_270:
            x = orig_y;
            y = dev->width - 1 - orig_x;
            break;
        default:
            break;
    }
    
    // Kiểm tra giới hạn
    if (x >= dev->width || y >= dev->height) return;
    
    size_t buffer_size = dev->width * dev->height / 8;
    uint32_t addr = (x + y * dev->width) / 8;
    uint8_t bit = 7 - (x % 8);
    
    if (addr >= buffer_size) return;
    
    switch (color) {
        case EPD_COLOR_BLACK:
            if (dev->swap_red_black) {
                dev->frame_buffer_red[addr] &= ~(1 << bit);  // Đỏ = 0
                dev->frame_buffer_black[addr] |= (1 << bit);  // Đen = 1 (trắng)
            } else {
                dev->frame_buffer_black[addr] &= ~(1 << bit);  // Đen = 0
                dev->frame_buffer_red[addr] |= (1 << bit);     // Đỏ = 1 (không đỏ)
            }
            break;
            
        case EPD_COLOR_WHITE:
            dev->frame_buffer_black[addr] |= (1 << bit);  // Đen = 1 (trắng)
            dev->frame_buffer_red[addr] |= (1 << bit);    // Đỏ = 1 (không đỏ)
            break;
            
        case EPD_COLOR_RED:
            if (dev->swap_red_black) {
                dev->frame_buffer_black[addr] &= ~(1 << bit);  // Đen = 0
                dev->frame_buffer_red[addr] |= (1 << bit);     // Đỏ = 1 (không đỏ)
            } else {
                dev->frame_buffer_red[addr] &= ~(1 << bit);    // Đỏ = 0
                dev->frame_buffer_black[addr] |= (1 << bit);   // Đen = 1 (trắng)
            }
            break;
    }
}

// Chế độ sleep
void epd_sleep(epd_handle_t handle) {
    epd_write_command(handle, EPD_CMD_DEEP_SLEEP_MODE);
    epd_write_data_byte(handle, 0x01);  // Enter deep sleep
    vTaskDelay(pdMS_TO_TICKS(100));
}

// Giải phóng tài nguyên
void epd_deinit(epd_handle_t handle) {
    if (!handle) return;
    
    epd_dev_t *dev = (epd_dev_t *)handle;
    
    epd_sleep(handle);
    
    if (dev->frame_buffer_black) free(dev->frame_buffer_black);
    if (dev->frame_buffer_red) free(dev->frame_buffer_red);
    
    spi_bus_remove_device(dev->spi);
    free(dev);
}