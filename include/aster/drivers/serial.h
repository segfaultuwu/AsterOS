#ifndef ASTER_DRIVERS_SERIAL_H
#define ASTER_DRIVERS_SERIAL_H

#include <stdint.h>

void serial_init(void);
void serial_putchar(char c);
void serial_puts(const char *str);

#endif // SERIAL_H
