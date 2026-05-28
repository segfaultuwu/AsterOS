#include "aster/drivers/console/font.h"
#include "aster/drivers/framebuffer.h"

#include <stdint.h>
void console_draw_char(char c, uint64_t x, uint64_t y, uint32_t fg, uint32_t bg) {
    const uint8_t *glyph = font_get_glyph(c);

    if (!glyph) {
        return;
    }

    uint8_t width = font_get_width();
    uint8_t height = font_get_height();

    for (uint8_t row = 0; row < height; row++) {
        uint8_t bits = glyph[row];

        for (uint8_t col = 0; col < width; col++) {
            uint32_t color;

            if (bits & (0x80 >> col)) {
                color = fg;
            } else {
                color = bg;
            }

            framebuffer_put_pixel(x + col, y + row, color);
        }
    }
}

void console_draw_string(const char *str, uint64_t x, uint64_t y, uint32_t fg, uint32_t bg) {
    uint64_t start_x = x;
    uint8_t char_width = font_get_width();
    uint8_t char_height = font_get_height();

    while (*str) {
        if (*str == '\n') {
            x = start_x;
            y += char_height;
        } else {
            console_draw_char(*str, x, y, fg, bg);
            x += char_width;
        }

        str++;
    }
}
