#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "pn5180.h"

static const char *TAG = "PN5180_TEST";

void print_hex(const char *label, const uint8_t *data, size_t len) {
    printf("%-15s: ", label);
    for (int i = 0; i < len; i++) {
        printf("%02X", data[i]);
        if (i < len - 1) printf(":");
    }
    printf("\n");
}

void print_binary(uint32_t value) {
    printf("Binary: ");
    for (int i = 31; i >= 0; i--) {
        printf("%ld", (value >> i) & 1);
        if (i % 8 == 0 && i != 0) printf(".");
    }
    printf("\n");
}

void test_spi_configuration(void) {
    printf("\nSPI CONFIGURATION TEST\n");
    printf("========================\n");
    
    // 1. Configure pins for ESP32-S3
    pn5180_config_t config = {
        .spi_host = SPI2_HOST,      // Using SPI2 on ESP32-S3
        .cs_pin = 9,                // GPIO9 - Chip Select
        .rst_pin = 8,               // GPIO8 - Reset
        .busy_pin = 5,              // GPIO5 - BUSY (active HIGH)
        .irq_pin = 4,               // GPIO4 - IRQ (optional)
        .clock_speed_hz = 1000000,  // 1MHz for stable communication
    };
    
    printf("Configuration:\n");
    printf("  SPI Host:   SPI2\n");
    printf("  CS Pin:     GPIO%d\n", config.cs_pin);
    printf("  RST Pin:    GPIO%d\n", config.rst_pin);
    printf("  BUSY Pin:   GPIO%d (PULL-DOWN enabled)\n", config.busy_pin);
    printf("  IRQ Pin:    GPIO%d\n", config.irq_pin);
    printf("  SPI Speed:  %lu Hz\n", config.clock_speed_hz);
    printf("\n");
    
    // 2. Initialize PN5180
    printf("Initializing PN5180...\n");
    pn5180_handle_t nfc = pn5180_init(&config);
    
    if (!nfc) {
        printf("FAILED: PN5180 initialization failed!\n");
        printf("Possible causes:\n");
        printf("  1. Wiring issues (check MOSI/MISO/SCK/CS/RST/BUSY)\n");
        printf("  2. Power supply (PN5180 needs stable 3.3V)\n");
        printf("  3. BUSY pin stuck HIGH\n");
        printf("  4. SPI communication failure\n");
        return;
    }
    
    printf("✅ SUCCESS: PN5180 initialized\n\n");
    
    // 3. Test BUSY pin functionality
    printf("3. BUSY PIN TEST\n");
    printf("   Current BUSY state: %s\n", 
           pn5180_is_busy(nfc) ? "HIGH (busy)" : "LOW (ready)");
    
    if (pn5180_is_busy(nfc)) {
        printf("   Waiting for ready... ");
        if (pn5180_wait_ready(nfc, 1000)) {
            printf("READY\n");
        } else {
            printf("TIMEOUT! Disabling BUSY checking...\n");
            pn5180_set_busy_checking(nfc, false);
        }
    }
    
    // 4. Read PN5180 version
    printf("\n4. PN5180 VERSION INFORMATION\n");
    uint16_t product, firmware, eeprom;
    pn5180_get_version(nfc, &product, &firmware, &eeprom);
    
    printf("   Product Version:   0x%04X\n", product);
    printf("   Firmware Version:  0x%04X\n", firmware);
    printf("   EEPROM Version:    0x%04X\n", eeprom);
    
    if (product == 0x0012 || product == 0x0013) {
        printf("Valid PN5180 detected\n");
    } else if (product == 0xFFFF || product == 0x0000) {
        printf("   ⚠️  Suspect SPI communication issues\n");
    }
    
    // 5. Read important registers
    printf("\n5. REGISTER DUMP\n");
    
    uint32_t regs[] = {0x00, 0x01, 0x02, 0x03, 0x11, 0x13, 0x16};
    const char *reg_names[] = {
        "SYSTEM_CONFIG", "IRQ_ENABLE", "IRQ_STATUS", 
        "IRQ_STATUS", "RF_STATUS", "RX_CONFIG", "RF_CONTROL"
    };
    
    for (int i = 0; i < 7; i++) {
        uint32_t value = pn5180_read_register(nfc, regs[i]);
        printf("   0x%02lX %-15s: 0x%08lX ", regs[i], reg_names[i], value);
        
        if (value == 0xFFFFFFFF) {
            printf("❌ (SPI error)\n");
        } else if (value == 0x00000000) {
            printf("(suspicious)\n");
        } else {
            printf("Done\n");
        }
    }
    
    // 6. Test RF configurations
    printf("\n6. RF CONFIGURATION TEST\n");
    
    pn5180_rf_config_t rf_configs[] = {
        PN5180_RF_CONFIG_14443A_106KBPS,
        PN5180_RF_CONFIG_15693_26KBPS,
    };
    
    const char *rf_names[] = {
        "ISO14443A (106kbps)",
        "ISO15693 (26kbps)",
    };
    
    for (int i = 0; i < 2; i++) {
        printf("   Loading %s... ", rf_names[i]);
        pn5180_load_rf_config(nfc, rf_configs[i]);
        printf("OK\n");
    }
    
    // 7. Main NFC scanning loop
    printf("\n7. NFC SCANNING TEST\n");
    printf("   Place NFC tag near antenna...\n");
    printf("   Press Ctrl+C to stop\n");
    printf("   ---------------------------------\n");
    
    uint32_t scan_count = 0;
    uint32_t tag_detections = 0;
    
    while (1) {
        scan_count++;
        
        printf("\n[Scan #%lu] ", scan_count);
        
        // Wait for PN5180 ready
        if (!pn5180_wait_ready(nfc, 1000)) {
            printf("PN5180 not ready, skipping...\n");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        
        bool tag_found = false;
        
        // ===== ISO14443A (MIFARE/NTAG) SCAN =====
        printf("ISO14443A... ");
        pn5180_load_rf_config(nfc, PN5180_RF_CONFIG_14443A_106KBPS);
        pn5180_rf_field_on(nfc);
        
        if (pn5180_inventory_14443a(nfc)) {
            uint8_t uid[10];
            uint8_t uid_len;
            
            if (pn5180_read_uid_14443a(nfc, uid, &uid_len)) {
                printf("TAG DETECTED!\n");
                tag_detections++;
                tag_found = true;
                
                printf("      Type:    ISO14443A (MIFARE/NTAG)\n");
                printf("      UID Len: %d bytes\n", uid_len);
                print_hex("UID", uid, uid_len);
                
                // Extra information
                printf("      UID Decimal: ");
                for (int i = 0; i < uid_len; i++) {
                    printf("%03d", uid[i]);
                    if (i < uid_len - 1) printf(".");
                }
                printf("\n");
                
                // Calculate checksum
                uint8_t checksum = 0;
                for (int i = 0; i < uid_len; i++) {
                    checksum ^= uid[i];
                }
                printf("      Checksum:    0x%02X\n", checksum);
            }
        } else {
            printf("✗\n");
        }
        
        pn5180_rf_field_off(nfc);
        
        // ===== ISO15693 SCAN (if no ISO14443A found) =====
        if (!tag_found) {
            printf("      ISO15693... ");
            pn5180_load_rf_config(nfc, PN5180_RF_CONFIG_15693_26KBPS);
            pn5180_rf_field_on(nfc);
            
            uint8_t uid_15693[8] = {0};
            if (pn5180_inventory_15693(nfc, uid_15693)) {
                printf("TAG DETECTED!\n");
                tag_detections++;
                tag_found = true;
                
                printf("      Type: ISO15693 (VICC)\n");
                print_hex("UID", uid_15693, 8);
                
                // Show reversed UID (ISO15693 standard)
                printf("      UID Reversed: ");
                for (int i = 7; i >= 0; i--) {
                    printf("%02X", uid_15693[i]);
                }
                printf("\n");
            } else {
                printf("✗\n");
            }
            
            pn5180_rf_field_off(nfc);
        }
        
        // ===== STATUS DISPLAY =====
        if (!tag_found) {
            printf("      No tag detected\n");
        }
        
        // Display statistics every 10 scans
        if (scan_count % 10 == 0) {
            printf("\nSTATISTICS\n");
            printf("   Total scans:      %lu\n", scan_count);
            printf("   Tags detected:    %lu\n", tag_detections);
            printf("   Detection rate:   %.1f%%\n", 
                   (tag_detections * 100.0) / scan_count);
            
            // Memory usage
            printf("   Free heap:        %lu bytes\n", esp_get_free_heap_size());
            
            // Uptime
            TickType_t ticks = xTaskGetTickCount();
            uint32_t seconds = ticks / configTICK_RATE_HZ;
            printf("   Uptime:           %02lu:%02lu:%02lu\n", 
                   seconds / 3600, (seconds % 3600) / 60, seconds % 60);
        }
        
        // Delay between scans
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    
    // Cleanup (will never reach here in infinite loop)
    pn5180_deinit(nfc);
}

void app_main(void) {
    printf("\n");
    printf("===========================================\n");
    printf("     PN5180 NFC READER COMPLETE TEST\n");
    printf("     ESP32-S3 + ESP-IDF v5.1.6\n");
    printf("===========================================\n");
    printf("\n");
    
    // Start the main test
    test_spi_configuration();
    
    printf("\n===========================================\n");
    printf("Test complete. Monitor output for NFC tags.\n");
    printf("===========================================\n");
}