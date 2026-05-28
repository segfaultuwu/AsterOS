#include <stdint.h>
#include <stdbool.h>

#include "aster/drivers/keyboard.h"
#include "aster/drivers/console/console.h"
#include "aster/drivers/ports.h"
#include "aster/arch/x86_64/idt.h"
#include "aster/drivers/serial.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define KEYBOARD_BUFFER_SIZE 256

static const char scancode_set1_map[] = {
    0,  27, '1', '2', '3', '4', '5', '6',
    '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u',
    'i', 'o', 'p', '[', ']', '\n',
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j',
    'k', 'l', ';', '\'', '`',
    0,  '\\', 'z', 'x', 'c', 'v', 'b', 'n',
    'm', ',', '.', '/',
    0,  '*',
    0,  ' '
};

static bool shift_pressed = false;
static bool caps_lock = false;
static bool extended_scancode = false;

static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static uint32_t buffer_head = 0;
static uint32_t buffer_tail = 0;

static bool buffer_is_full(void) {
    return ((buffer_head + 1) % KEYBOARD_BUFFER_SIZE) == buffer_tail;
}

static void buffer_push(char c) {
    if (buffer_is_full()) {
        // Drop oldest char.
        buffer_tail = (buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
    }

    keyboard_buffer[buffer_head] = c;
    buffer_head = (buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
}

static char apply_shift(char c) {
    switch (c) {
        case '1': return '!';
        case '2': return '@';
        case '3': return '#';
        case '4': return '$';
        case '5': return '%';
        case '6': return '^';
        case '7': return '&';
        case '8': return '*';
        case '9': return '(';
        case '0': return ')';
        case '-': return '_';
        case '=': return '+';
        case '[': return '{';
        case ']': return '}';
        case ';': return ':';
        case '\'': return '"';
        case '`': return '~';
        case '\\': return '|';
        case ',': return '<';
        case '.': return '>';
        case '/': return '?';
        default: return c;
    }
}

static void keyboard_flush_output(void) {
    while (inb(KEYBOARD_STATUS_PORT) & 0x01) {
        (void)inb(KEYBOARD_DATA_PORT);
    }
}

void keyboard_init(void) {
    buffer_head = 0;
    buffer_tail = 0;

    shift_pressed = false;
    caps_lock = false;
    extended_scancode = false;

    keyboard_flush_output();

    idt_register_irq_handler(1, keyboard_handler);
}

static void keyboard_process_scancode(uint8_t scancode) {
    // Extended scancode prefix.
    if (scancode == 0xE0) {
        extended_scancode = true;
        return;
    }

    // For now ignore extended keys: arrows, right ctrl, right alt itd.
    if (extended_scancode) {
        extended_scancode = false;
        return;
    }

    bool released = (scancode & 0x80) != 0;
    uint8_t key = scancode & 0x7F;

    if (released) {
        if (key == 0x2A || key == 0x36) {
            shift_pressed = false;
        }

        return;
    }

    switch (key) {
        case 0x2A:
        case 0x36:
            shift_pressed = true;
            return;

        case 0x3A:
            caps_lock = !caps_lock;
            return;

        default:
            break;
    }

    char c = 0;

    if (key < sizeof(scancode_set1_map)) {
        c = scancode_set1_map[key];
    }

    if (c == 0) {
        return;
    }

    if (c >= 'a' && c <= 'z') {
        bool uppercase = shift_pressed ^ caps_lock;

        if (uppercase) {
            c = (char)(c - 'a' + 'A');
        }
    } else if (shift_pressed) {
        c = apply_shift(c);
    }

    if (c != 0) {
        buffer_push(c);
    }
}

void keyboard_poll(void) {
    while (inb(KEYBOARD_STATUS_PORT) & 0x01) {
        keyboard_process_scancode(inb(KEYBOARD_DATA_PORT));
    }
}

void keyboard_handler(registers_t *regs) {
    (void)regs;

    keyboard_process_scancode(inb(KEYBOARD_DATA_PORT));
}

char keyboard_read(void) {
    if (buffer_head == buffer_tail) {
        return 0;
    }

    char c = keyboard_buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;

    return c;
}

bool keyboard_has_data(void) {
    return buffer_head != buffer_tail;
}
