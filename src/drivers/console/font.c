#include "aster/drivers/console/font.h"

extern uint8_t _binary_font_psf_start[];
extern uint8_t _binary_font_psf_end[];
extern uint8_t _binary_font_psf_size[];

#define PSF1_MAGIC0 0x36
#define PSF1_MAGIC1 0x04

static psf1_header_t *font_header = 0;
static uint8_t *glyphs = 0;
static uint8_t font_height = 0;
static uint8_t font_width = 8;
static uint16_t glyph_count = 0;

bool font_init(void) {
    font_header = (psf1_header_t *)_binary_font_psf_start;

    if (font_header->magic[0] != PSF1_MAGIC0 || font_header->magic[1] != PSF1_MAGIC1) {
        return false;
    }

    font_height = font_header->charsize;
    font_width = 8;

    if (font_header->mode & 0x01) {
        glyph_count = 512;
    } else {
        glyph_count = 256;
    }

    glyphs = _binary_font_psf_start + sizeof(psf1_header_t);

    return true;
}

const uint8_t *font_get_glyph(char c) {
    if (!glyphs) {
        return 0;
    }

    uint8_t index = (uint8_t)c;

    if (index >= glyph_count) {
        index = '?';
    }

    return glyphs + index * font_height;
}

uint8_t font_get_width(void) {
    return font_width;
}

uint8_t font_get_height(void) {
    return font_height;
}
