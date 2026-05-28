#ifndef ASTER_DRIVERS_CONSOLE_H
#define ASTER_DRIVERS_CONSOLE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

bool console_init(void);
void console_write(const char *str);
void console_putc(char c);
void console_clear(void);

#endif
