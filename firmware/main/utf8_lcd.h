#ifndef UTF8_LCD_H
#define UTF8_LCD_H

#include <stdint.h>


void lcd_show_utf8_wrapped(
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    const char *text,
    uint16_t color,
    uint16_t background
);

#endif
