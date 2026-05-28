#ifndef ASTER_LIB_KPRINTF_H
#define ASTER_LIB_KPRINTF_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

typedef void (*kprintf_putc_t)(char c, void *ctx);

void kvprintf(kprintf_putc_t putc, void *ctx, const char *fmt, va_list args);
void kprintf_to(kprintf_putc_t putc, void *ctx, const char *fmt, ...);

#endif
