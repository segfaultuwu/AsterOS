#include <stdint.h>
#include <stdbool.h>
#include "aster/drivers/keyboard.h"
#include "aster/drivers/ports.h"
#include "aster/arch/x86_64/idt.h"

// Keyboard scancode map (US layout)
static const char scancode_map[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0, ' '
};

static bool shift_pressed = false;
static bool caps_lock = false;

// Keyboard buffer
#define KEYBOARD_BUFFER_SIZE 256
static char keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static uint32_t buffer_head = 0;
static uint32_t buffer_tail = 0;

// Initialize keyboard
void keyboard_init() {
    // Register IRQ1 handler (keyboard interrupt)
    idt_register_irq_handler(1, keyboard_handler);

    // Clear keyboard buffer
    buffer_head = buffer_tail = 0;
}

// Keyboard interrupt handler
void keyboard_handler(registers_t* regs) {
    (void)regs;

    uint8_t scancode = inb(0x60);

    // Handle key release
    if (scancode & 0x80) {
        scancode &= 0x7F;

        // Handle shift release
        if (scancode == 0x2A || scancode == 0x36) {
            shift_pressed = false;
        }
        return;
    }

    // Handle special keys
    switch (scancode) {
        case 0x2A: // Left shift
        case 0x36: // Right shift
            shift_pressed = true;
            return;

        case 0x3A: // Caps lock
            caps_lock = !caps_lock;
            return;

        default:
            break;
    }

    // Get character from scancode map
    char c = scancode_map[scancode];

    // Handle shift and caps lock
    if (shift_pressed || caps_lock) {
        if (c >= 'a' && c <= 'z') {
            c = c - 'a' + 'A';
        } else if (c >= '0' && c <= '9') {
            static const char shift_numbers[] = ")!@#$%^&*(";
            c = shift_numbers[c - '0'];
        } else {
            switch (c) {
                case '1': c = '!'; break;
                case '2': c = '@'; break;
                case '3': c = '#'; break;
                case '4': c = '$'; break;
                case '5': c = '%'; break;
                case '6': c = '^'; break;
                case '7': c = '&'; break;
                case '8': c = '*'; break;
                case '9': c = '('; break;
                case '0': c = ')'; break;
                case '-': c = '_'; break;
                case '=': c = '+'; break;
                case '[': c = '{'; break;
                case ']': c = '}'; break;
                case ';': c = ':'; break;
                case '\'': c = '"'; break;
                case '`': c = '~'; break;
                case '\\': c = '|'; break;
                case ',': c = '<'; break;
                case '.': c = '>'; break;
                case '/': c = '?'; break;
                default: break;
            }
        }
    }

    // Add character to buffer
    if (c != 0) {
        keyboard_buffer[buffer_head] = c;
        buffer_head = (buffer_head + 1) % KEYBOARD_BUFFER_SIZE;

        // Display the character on screen
        static uint32_t x = 0, y = 0;
        if (c == '\n') {
            x = 0;
            y++;
        } else if (c == '\b') {
            if (x > 0) x--;
            // TODO: Print to screen
        } else {
            // TODO: Print to screen
            x++;
            if (x >= 80) {
                x = 0;
                y++;
            }
            if (y >= 25) {
                y = 0;
            }
        }
    }
}

// Read character from keyboard buffer
char keyboard_read() {
    if (buffer_head == buffer_tail) {
        return 0; // Buffer empty
    }

    char c = keyboard_buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}

// Check if keyboard buffer has data
bool keyboard_has_data() {
    return buffer_head != buffer_tail;
}
