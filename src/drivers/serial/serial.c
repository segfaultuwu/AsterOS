#include "aster/drivers/serial.h"
#include "aster/drivers/ports.h"

#define COM1 0x3F8

static int serial_is_transmit_empty(void) {
    return inb(COM1 + 5) & 0x20;
}

void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

void serial_putchar(char c) {
    while (!serial_is_transmit_empty()) {
        __asm__ volatile ("pause");
    }

    outb(COM1, (uint8_t)c);
}

void serial_puts(const char *str) {
    while (*str) {
        if (*str == '\n') {
            serial_putchar('\r');
        }

        serial_putchar(*str);
        str++;
    }
}
