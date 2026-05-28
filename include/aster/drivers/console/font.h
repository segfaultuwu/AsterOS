#ifndef FONT_H
#define FONT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint8_t magic[2];
    uint8_t mode;
    uint8_t charsize;
} __attribute__((packed)) psf1_header_t;

bool font_init(void);

const uint8_t *font_get_glyph(char c);
uint8_t font_get_width(void);
uint8_t font_get_height(void);

#endif
