#ifndef LCD_H
#define LCD_H

#include <stdint.h>

/**
 * @brief Initialize GPIOs and LCD controller (4-bit mode)
 */
void lcd_init(void);

/**
 * @brief Send a string to the current cursor position
 */
void lcd_put_str(const char *str);

/**
 * @brief Send a raw command (mode 0) or data byte (mode 1)
 */
void lcd_send_byte(uint8_t data, uint8_t mode);

/**
 * @brief Clear the display and return cursor to home
 */
void lcd_clear(void);

/**
 * @brief Move cursor to specific line (0-1) and column (0-7)
 */
void lcd_goto(uint8_t line, uint8_t col);

#endif