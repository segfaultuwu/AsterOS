#include "aster/debug/logging.h"

#include <stdarg.h>

#include "aster/drivers/console/console.h"
#include "aster/drivers/serial.h"
#include "aster/lib/kprintf.h"

static bool logging_initialized = false;

static void log_serial_prefix(log_level_t level) {
    switch (level) {
        case LOG_LEVEL_INFO:
            serial_puts("[INFO] ");
            break;

        case LOG_LEVEL_OK:
            serial_puts("[ OK ] ");
            break;

        case LOG_LEVEL_WARN:
            serial_puts("[WARN] ");
            break;

        case LOG_LEVEL_ERROR:
            serial_puts("[ERR ] ");
            break;

        case LOG_LEVEL_PANIC:
            serial_puts("[PANIC] ");
            break;
    }
}

static void log_console_prefix(log_level_t level) {
    switch (level) {
        case LOG_LEVEL_INFO:
            console_write("\033[38;2;137;180;250m");
            console_write("[INFO] ");
            break;

        case LOG_LEVEL_OK:
            console_write("\033[38;2;166;227;161m");
            console_write("[ OK ] ");
            break;

        case LOG_LEVEL_WARN:
            console_write("\033[38;2;249;226;175m");
            console_write("[WARN] ");
            break;

        case LOG_LEVEL_ERROR:
            console_write("\033[38;2;243;139;168m");
            console_write("[ERR ] ");
            break;

        case LOG_LEVEL_PANIC:
            console_write("\033[38;2;243;139;168m");
            console_write("[PANIC] ");
            break;
    }

    console_write("\033[38;2;205;214;244m");
}

static void log_putc_callback(char c, void *ctx) {
    (void)ctx;

    serial_putchar(c);

    if (logging_initialized) {
        console_putc(c);
    }
}

static void log_vprintf(log_level_t level, const char *fmt, va_list args) {
    if (fmt == 0) {
        return;
    }

    log_serial_prefix(level);

    if (logging_initialized) {
        log_console_prefix(level);
    }

    kvprintf(log_putc_callback, 0, fmt, args);

    serial_puts("\n");

    if (logging_initialized) {
        console_write("\033[0m\n");
    }
}

void log_init(void) {
    logging_initialized = true;
}

void log_write(log_level_t level, const char *message) {
    if (message == 0) {
        return;
    }

    log_serial_prefix(level);
    serial_puts(message);
    serial_puts("\n");

    if (logging_initialized) {
        log_console_prefix(level);
        console_write(message);
        console_write("\033[0m\n");
    }
}

void log_printf(log_level_t level, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    log_vprintf(level, fmt, args);
    va_end(args);
}

void log_info(const char *message) {
    log_write(LOG_LEVEL_INFO, message);
}

void log_ok(const char *message) {
    log_write(LOG_LEVEL_OK, message);
}

void log_warn(const char *message) {
    log_write(LOG_LEVEL_WARN, message);
}

void log_error(const char *message) {
    log_write(LOG_LEVEL_ERROR, message);
}

void log_panic(const char *message) {
    log_write(LOG_LEVEL_PANIC, message);

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

void log_infof(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    log_vprintf(LOG_LEVEL_INFO, fmt, args);
    va_end(args);
}

void log_okf(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    log_vprintf(LOG_LEVEL_OK, fmt, args);
    va_end(args);
}

void log_warnf(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    log_vprintf(LOG_LEVEL_WARN, fmt, args);
    va_end(args);
}

void log_errorf(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    log_vprintf(LOG_LEVEL_ERROR, fmt, args);
    va_end(args);
}

void log_panicf(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    log_vprintf(LOG_LEVEL_PANIC, fmt, args);
    va_end(args);

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

void log_hex64(const char *label, uint64_t value) {
    if (label == 0) {
        label = "value";
    }

    log_infof("%s = 0x%llx", label, value);
}

void log_u64(const char *label, uint64_t value) {
    if (label == 0) {
        label = "value";
    }

    log_infof("%s = %llu", label, value);
}
