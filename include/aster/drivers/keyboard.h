#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>
#include "aster/arch/x86_64/idt.h"

// Initialize keyboard
void keyboard_init();

// Keyboard interrupt handler
void keyboard_handler(registers_t* regs);

// Read character from keyboard buffer
char keyboard_read();

// Check if keyboard buffer has data
bool keyboard_has_data();

#endif // KEYBOARD_H
