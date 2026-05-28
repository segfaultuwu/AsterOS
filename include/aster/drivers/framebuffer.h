#ifndef ASTER_DRIVERS_FRAMEBUFFER_H
#define ASTER_DRIVERS_FRAMEBUFFER_H

#include <stdint.h>
#include <stdbool.h>

void framebuffer_init(void);
bool framebuffer_is_active(void);

uint64_t framebuffer_width(void);
uint64_t framebuffer_height(void);
uint64_t framebuffer_pitch(void);

void framebuffer_put_pixel(uint64_t x, uint64_t y, uint32_t color);
void framebuffer_clear(uint32_t color);

#endif
