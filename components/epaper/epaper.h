#ifndef EPAPER_H
#define EPAPER_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/spi_master.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// Kích thước màn hình Pico ePaper 2.66 inch
#define EPD_WIDTH   152     // Chiều rộng (chiều ngang)
#define EPD_HEIGHT  296     // Chiều cao (chiều dọc)

// Màu sắc
typedef enum {
    EPD_COLOR_BLACK = 0,
    EPD_COLOR_WHITE = 1,
    EPD_COLOR_RED = 2
} epd_color_t;

// Chiều hiển thị
typedef enum {
    EPD_ROTATE_0 = 0,
    EPD_ROTATE_90,
    EPD_ROTATE_180,
    EPD_ROTATE_270
} epd_rotation_t;

// Cấu trúc cấu hình
typedef struct {
    spi_host_device_t spi_host;    // SPI2_HOST hoặc SPI3_HOST
    int busy_pin;
    int rst_pin;
    int dc_pin;
    int cs_pin;
    int sck_pin;
    int mosi_pin;
    epd_rotation_t rotation;       // Chiều hiển thị
    bool swap_red_black;           // Đảo màu đỏ/đen nếu cần
} epd_config_t;

// Handle
typedef void* epd_handle_t;

// Khởi tạo và giải phóng
epd_handle_t epd_init(const epd_config_t *config);
void epd_deinit(epd_handle_t epd);

// Điều khiển cơ bản
void epd_reset(epd_handle_t epd);
void epd_sleep(epd_handle_t epd);
void epd_wakeup(epd_handle_t epd);
void epd_clear(epd_handle_t epd, epd_color_t color);

// Hiển thị
void epd_display_frame(epd_handle_t epd, const uint8_t *black_buffer, 
                       const uint8_t *red_buffer);
void epd_partial_display(epd_handle_t epd, uint16_t x, uint16_t y, 
                         uint16_t width, uint16_t height, 
                         const uint8_t *black_buffer, const uint8_t *red_buffer);

// Vẽ cơ bản
void epd_draw_pixel(epd_handle_t epd, uint16_t x, uint16_t y, epd_color_t color);
void epd_draw_line(epd_handle_t epd, uint16_t x0, uint16_t y0, 
                   uint16_t x1, uint16_t y1, epd_color_t color);
void epd_draw_rectangle(epd_handle_t epd, uint16_t x, uint16_t y, 
                        uint16_t width, uint16_t height, epd_color_t color, bool filled);
void epd_draw_circle(epd_handle_t epd, uint16_t x, uint16_t y, 
                     uint16_t radius, epd_color_t color, bool filled);

// Vẽ text (cần include fonts.h)
typedef struct {
    const uint8_t *data;
    uint16_t width;
    uint16_t height;
    uint8_t first_char;
    uint8_t last_char;
    const uint16_t *char_widths;
    uint8_t char_height;
} epd_font_t;

void epd_draw_char(epd_handle_t epd, uint16_t x, uint16_t y, 
                   char ch, const epd_font_t *font, epd_color_t color);
void epd_draw_string(epd_handle_t epd, uint16_t x, uint16_t y, 
                     const char *str, const epd_font_t *font, epd_color_t color);

// Hình ảnh
void epd_draw_bitmap(epd_handle_t epd, uint16_t x, uint16_t y, 
                     uint16_t width, uint16_t height, 
                     const uint8_t *black_data, const uint8_t *red_data);

// Tiện ích
void epd_get_dimensions(epd_handle_t epd, uint16_t *width, uint16_t *height);
void epd_set_rotation(epd_handle_t epd, epd_rotation_t rotation);
bool epd_is_busy(epd_handle_t epd);

#ifdef __cplusplus
}
#endif

#endif // EPAPER_H