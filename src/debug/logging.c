#include "aster/debug/logging.h"

#include "aster/drivers/console/console.h"
#include "aster/drivers/serial.h"

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

static void serial_put_u64(uint64_t value) {
    char buf[32];
    int i = 0;

    if (value == 0) {
        serial_putchar('0');
        return;
    }

    while (value > 0) {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0) {
        serial_putchar(buf[--i]);
    }
}

static void console_put_u64(uint64_t value) {
    char buf[32];
    int i = 0;

    if (value == 0) {
        console_putc('0');
        return;
    }

    while (value > 0) {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i > 0) {
        console_putc(buf[--i]);
    }
}

static void serial_put_hex_u64(uint64_t value) {
    const char *hex = "0123456789ABCDEF";

    serial_puts("0x");

    for (int i = 60; i >= 0; i -= 4) {
        serial_putchar(hex[(value >> i) & 0xF]);
    }
}

static void console_put_hex_u64(uint64_t value) {
    const char *hex = "0123456789ABCDEF";

    console_write("0x");

    for (int i = 60; i >= 0; i -= 4) {
        console_putc(hex[(value >> i) & 0xF]);
    }
}

void log_init(void) {
    logging_initialized = true;
}

void log_write(log_level_t level, const char *message) {
    if (message == NULL) {
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
        __asm__ volatile ("cli");
        __asm__ volatile ("hlt");
    }
}

void log_hex64(const char *label, uint64_t value) {
    if (label == NULL) {
        label = "value";
    }

    serial_puts("[INFO] ");
    serial_puts(label);
    serial_puts(" = ");
    serial_put_hex_u64(value);
    serial_puts("\n");

    if (logging_initialized) {
        console_write("\033[38;2;137;180;250m[INFO] ");
        console_write("\033[38;2;205;214;244m");
        console_write(label);
        console_write(" = ");
        console_put_hex_u64(value);
        console_write("\033[0m\n");
    }
}

void log_u64(const char *label, uint64_t value) {
    if (label == NULL) {
        label = "value";
    }

    serial_puts("[INFO] ");
    serial_puts(label);
    serial_puts(" = ");
    serial_put_u64(value);
    serial_puts("\n");

    if (logging_initialized) {
        console_write("\033[38;2;137;180;250m[INFO] ");
        console_write("\033[38;2;205;214;244m");
        console_write(label);
        console_write(" = ");
        console_put_u64(value);
        console_write("\033[0m\n");
    }
}
