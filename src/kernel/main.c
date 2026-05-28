#include <stdint.h>
#include <stdbool.h>

#include "aster/drivers/console/console.h"
#include "aster/drivers/framebuffer.h"
#include "aster/drivers/serial.h"
#include "aster/drivers/framebuffer.h"

#define CAT_MOCHA_BASE 0x1E1E2E
#define CAT_MOCHA_RED  0xF38BA8
#define CAT_MOCHA_GREEN 0xA6E3A1
#define CAT_MOCHA_BLUE 0x89B4FA

void kmain(void) {
    serial_init();
    serial_puts("kmain reached\n");

    framebuffer_init();

    if (!framebuffer_is_active()) {
        serial_puts("framebuffer not active\n");

        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    serial_puts("framebuffer active\n");

    framebuffer_clear(CAT_MOCHA_BASE);

    for (uint64_t y = 50; y < 150; y++) {
        for (uint64_t x = 50; x < 300; x++) {
            framebuffer_put_pixel(x, y, CAT_MOCHA_RED);
        }
    }

    for (uint64_t y = 180; y < 280; y++) {
        for (uint64_t x = 50; x < 300; x++) {
            framebuffer_put_pixel(x, y, CAT_MOCHA_GREEN);
        }
    }

    for (uint64_t x = 0; x < 600; x++) {
        framebuffer_put_pixel(x, 20, CAT_MOCHA_BLUE);
    }

    console_draw_string("hello, world!", 20, 20,  CAT_MOCHA_RED, CAT_MOCHA_GREEN);

    serial_puts("draw test done\n");

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
