#include "aster/drivers/console/console.h"
#include "aster/arch/x86_64/boot/limine.h"
#include "aster/drivers/serial.h"

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <flanterm.h>
#include <flanterm_backends/fb.h>

static struct flanterm_context *ft_ctx = 0;

static size_t console_strlen(const char *s) {
    size_t len = 0;

    while (s[len]) {
        len++;
    }

    return len;
}

/*
 * Prosty bump allocator dla Flanterm.
 * Bez tego niektóre wersje Flanterm potrafią paść w flanterm_fb_init().
 */
#define FLANTERM_HEAP_SIZE (64 * 1024 * 1024)

static uint8_t flanterm_heap[FLANTERM_HEAP_SIZE];
static size_t flanterm_heap_offset = 0;

static void *flanterm_alloc(size_t size) {
    serial_puts("flanterm_alloc\n");

    size = (size + 15) & ~((size_t)15);

    if (flanterm_heap_offset + size > FLANTERM_HEAP_SIZE) {
        serial_puts("flanterm_alloc: out of memory\n");
        return NULL;
    }

    void *ptr = &flanterm_heap[flanterm_heap_offset];
    flanterm_heap_offset += size;

    return ptr;
}

static void flanterm_free(void *ptr, size_t size) {
    (void)ptr;
    (void)size;
}

bool console_init(void) {
    serial_puts("console_init: enter\n");

    if (framebuffer_request.response == 0) {
        serial_puts("console_init: framebuffer response null\n");
        return false;
    }

    serial_puts("console_init: framebuffer response ok\n");

    if (framebuffer_request.response->framebuffer_count < 1) {
        serial_puts("console_init: framebuffer count zero\n");
        return false;
    }

    serial_puts("console_init: framebuffer count ok\n");

    struct limine_framebuffer *fb = framebuffer_request.response->framebuffers[0];

    if (fb == 0) {
        serial_puts("console_init: fb null\n");
        return false;
    }

    serial_puts("console_init: fb ptr ok\n");

    static uint32_t bg = 0x1E1E2E;
    static uint32_t fg = 0xCDD6F4;

    serial_puts("console_init: before flanterm_fb_init\n");

    ft_ctx = flanterm_fb_init(
        flanterm_alloc,
        flanterm_free,

        fb->address,
        fb->width,
        fb->height,
        fb->pitch,

        fb->red_mask_size,
        fb->red_mask_shift,
        fb->green_mask_size,
        fb->green_mask_shift,
        fb->blue_mask_size,
        fb->blue_mask_shift,

        NULL, // canvas
        NULL, // ansi colours
        NULL, // ansi bright colours

        NULL, // default background
        NULL, // default foreground

        NULL, // default background bright
        NULL, // default foreground bright

        NULL, // font

        0,    // font width
        0,    // font height
        1,    // font spacing

        0,    // margin
        0,    // margin gradient

        0,    // background
        0     // foreground
    );

    serial_puts("console_init: after flanterm_fb_init\n");

    if (ft_ctx == 0) {
        serial_puts("console_init: ft_ctx null\n");
        return false;
    }

    serial_puts("console_init: success\n");

    return true;
}

void console_putc(char c) {
    if (ft_ctx == 0) {
        return;
    }

    if (c == '\n') {
        char cr = '\r';
        flanterm_write(ft_ctx, &cr, 1);
    }

    flanterm_write(ft_ctx, &c, 1);
}

void console_write(const char *str) {
    if (ft_ctx == 0 || str == 0) {
        return;
    }

    while (*str) {
        console_putc(*str);
        str++;
    }
}

void console_clear(void) {
    if (ft_ctx == 0) {
        return;
    }

    console_write("\033[2J\033[H");
}
