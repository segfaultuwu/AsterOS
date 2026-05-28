#ifndef ASTER_DRIVERS_KEYBOARD_H
#define ASTER_DRIVERS_KEYBOARD_H

#include <stdbool.h>
#include "aster/arch/x86_64/idt.h"

void keyboard_init(void);
void keyboard_handler(registers_t *regs);
void keyboard_poll(void);

char keyboard_read(void);
bool keyboard_has_data(void);

#endif
