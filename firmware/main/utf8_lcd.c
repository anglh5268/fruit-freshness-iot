#include "utf8_lcd.h"

#include <stdbool.h>
#include <stddef.h>

#include "chinese_font_16.h"
#include "lcd.h"


#define UTF8_LCD_FONT_SIZE 16U
#define UTF8_LCD_ASCII_WIDTH 8U


static const chinese_glyph_16_t *find_glyph(uint32_t codepoint)
{
    size_t low = 0;
    size_t high = CHINESE_GLYPHS_16_COUNT;

    while (low < high) {
        size_t middle = low + (high - low) / 2;
        if (chinese_glyphs_16[middle].codepoint == codepoint) {
            return &chinese_glyphs_16[middle];
        }
        if (chinese_glyphs_16[middle].codepoint < codepoint) {
            low = middle + 1;
        } else {
            high = middle;
        }
    }
    return NULL;
}


static void draw_glyph(
    uint16_t x,
    uint16_t y,
    const uint8_t bitmap[32],
    uint16_t color,
    uint16_t background
)
{
    lcd_set_window(
        x,
        y,
        x + UTF8_LCD_FONT_SIZE - 1,
        y + UTF8_LCD_FONT_SIZE - 1
    );
    for (uint8_t row = 0; row < UTF8_LCD_FONT_SIZE; ++row) {
        for (uint8_t column = 0; column < UTF8_LCD_FONT_SIZE; ++column) {
            uint8_t byte = bitmap[row * 2U + column / 8U];
            uint8_t mask = (uint8_t)(0x80U >> (column % 8U));
            lcd_write_data16((byte & mask) != 0U ? color : background);
        }
    }
}


static void draw_missing_glyph(
    uint16_t x,
    uint16_t y,
    uint16_t color,
    uint16_t background
)
{
    lcd_fill(
        x,
        y,
        x + UTF8_LCD_FONT_SIZE - 1,
        y + UTF8_LCD_FONT_SIZE - 1,
        background
    );
    lcd_draw_rectangle(
        x + 2,
        y + 2,
        x + UTF8_LCD_FONT_SIZE - 3,
        y + UTF8_LCD_FONT_SIZE - 3,
        color
    );
}


static uint32_t decode_utf8(const unsigned char **cursor)
{
    const unsigned char *text = *cursor;
    uint32_t codepoint;

    if (text[0] < 0x80U) {
        *cursor = text + 1;
        return text[0];
    }
    if ((text[0] & 0xE0U) == 0xC0U &&
        (text[1] & 0xC0U) == 0x80U) {
        codepoint = ((uint32_t)(text[0] & 0x1FU) << 6) |
                    (uint32_t)(text[1] & 0x3FU);
        *cursor = text + 2;
        return codepoint;
    }
    if ((text[0] & 0xF0U) == 0xE0U &&
        (text[1] & 0xC0U) == 0x80U &&
        (text[2] & 0xC0U) == 0x80U) {
        codepoint = ((uint32_t)(text[0] & 0x0FU) << 12) |
                    ((uint32_t)(text[1] & 0x3FU) << 6) |
                    (uint32_t)(text[2] & 0x3FU);
        *cursor = text + 3;
        return codepoint;
    }

    *cursor = text + 1;
    return 0xFFFDU;
}


void lcd_show_utf8_wrapped(
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height,
    const char *text,
    uint16_t color,
    uint16_t background
)
{
    if (text == NULL || width < UTF8_LCD_ASCII_WIDTH ||
        height < UTF8_LCD_FONT_SIZE) {
        return;
    }

    const unsigned char *cursor = (const unsigned char *)text;
    uint16_t current_x = x;
    uint16_t current_y = y;
    const uint16_t x_end = x + width;
    const uint16_t y_end = y + height;

    while (*cursor != '\0' &&
           current_y + UTF8_LCD_FONT_SIZE <= y_end) {
        uint32_t codepoint = decode_utf8(&cursor);
        if (codepoint == '\r') {
            continue;
        }
        if (codepoint == '\n') {
            current_x = x;
            current_y += UTF8_LCD_FONT_SIZE;
            continue;
        }

        uint16_t character_width =
            codepoint < 0x80U ? UTF8_LCD_ASCII_WIDTH : UTF8_LCD_FONT_SIZE;
        if (current_x + character_width > x_end) {
            current_x = x;
            current_y += UTF8_LCD_FONT_SIZE;
            if (current_y + UTF8_LCD_FONT_SIZE > y_end) {
                break;
            }
        }

        if (codepoint >= 0x20U && codepoint <= 0x7EU) {
            lcd_show_char(
                current_x,
                current_y,
                (uint8_t)codepoint,
                UTF8_LCD_FONT_SIZE,
                0,
                color
            );
        } else {
            const chinese_glyph_16_t *glyph = find_glyph(codepoint);
            if (glyph != NULL) {
                draw_glyph(
                    current_x,
                    current_y,
                    glyph->bitmap,
                    color,
                    background
                );
            } else {
                draw_missing_glyph(current_x, current_y, color, background);
            }
        }
        current_x += character_width;
    }
}
