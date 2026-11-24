#include "lcd.h"
#include "driver/gpio.h"
#include "rom/ets_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// test: 33,13,14,27,26,27,32
// final: 3,9,10,11,12,13,14
// Controller: ST7066U

#define LCD_RS_PIN 3
#define LCD_RW_PIN 9
#define LCD_E_PIN 10
#define LCD_D4_PIN 11
#define LCD_D5_PIN 12
#define LCD_D6_PIN 13
#define LCD_D7_PIN 14

static void lcd_send_nibble(uint8_t nibble) {
    gpio_set_level(LCD_D4_PIN, (nibble >> 0) & 0x01);
    gpio_set_level(LCD_D5_PIN, (nibble >> 1) & 0x01);
    gpio_set_level(LCD_D6_PIN, (nibble >> 2) & 0x01);
    gpio_set_level(LCD_D7_PIN, (nibble >> 3) & 0x01);

    gpio_set_level(LCD_E_PIN, 1);
    ets_delay_us(25); 
    gpio_set_level(LCD_E_PIN, 0);
    ets_delay_us(100); 
}

void lcd_send_byte(uint8_t data, uint8_t mode) {
    gpio_set_level(LCD_RS_PIN, mode); 
    lcd_send_nibble(data >> 4);       
    lcd_send_nibble(data & 0x0F);    
}

void lcd_goto(uint8_t line, uint8_t col) {
    // Line 0 starts at 0x00, Line 1 starts at 0x40
    uint8_t address = (line == 0) ? (0x00 + col) : (0x40 + col);
    lcd_send_byte(0x80 | address, 0);
}

void lcd_clear(void) {
    lcd_send_byte(0x01, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
}

void lcd_init(void) {
    uint64_t pin_mask = (1ULL<<LCD_RS_PIN) | (1ULL<<LCD_RW_PIN) | (1ULL<<LCD_E_PIN) |
                        (1ULL<<LCD_D4_PIN) | (1ULL<<LCD_D5_PIN) | (1ULL<<LCD_D6_PIN) | (1ULL<<LCD_D7_PIN);
    
    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    gpio_set_level(LCD_RW_PIN, 0); // Always write mode
    vTaskDelay(pdMS_TO_TICKS(100)); // Give it plenty of time to power up
    
    // Reset sequence to force 4-bit mode
    lcd_send_nibble(0x03);
    vTaskDelay(pdMS_TO_TICKS(20)); // Increased delay
    lcd_send_nibble(0x03);
    vTaskDelay(pdMS_TO_TICKS(20));
    lcd_send_nibble(0x03);
    vTaskDelay(pdMS_TO_TICKS(20));
    
    lcd_send_nibble(0x02); // Finally set 4-bit mode
    vTaskDelay(pdMS_TO_TICKS(20));

    lcd_send_byte(0x28, 0); 
    lcd_send_byte(0x0C, 0); 
    lcd_send_byte(0x06, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    lcd_clear();
}

void lcd_put_str(const char *str) {
    while (*str) {
        lcd_send_byte(*str++, 1);
    }
}