#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <limine.h>

#include "aster/arch/x86_64/boot/limine.h"
#include "aster/drivers/framebuffer.h"

static struct limine_framebuffer *fb = NULL;

void framebuffer_init(void) {
    if (framebuffer_request.response == NULL) {
        fb = NULL;
        return;
    }

    if (framebuffer_request.response->framebuffer_count < 1) {
        fb = NULL;
        return;
    }

    fb = framebuffer_request.response->framebuffers[0];
}

bool framebuffer_is_active(void) {
    return fb != NULL;
}

uint64_t framebuffer_width(void) {
    return fb ? fb->width : 0;
}

uint64_t framebuffer_height(void) {
    return fb ? fb->height : 0;
}

uint64_t framebuffer_pitch(void) {
    return fb ? fb->pitch : 0;
}

void framebuffer_put_pixel(uint64_t x, uint64_t y, uint32_t color) {
    if (fb == NULL) {
        return;
    }

    if (x >= fb->width || y >= fb->height) {
        return;
    }

    uint8_t *pixel_addr = (uint8_t *)fb->address + y * fb->pitch + x * 4;
    *(uint32_t *)pixel_addr = color;
}

void framebuffer_clear(uint32_t color) {
    if (fb == NULL) {
        return;
    }

    for (uint64_t y = 0; y < fb->height; y++) {
        for (uint64_t x = 0; x < fb->width; x++) {
            framebuffer_put_pixel(x, y, color);
        }
    }
}
