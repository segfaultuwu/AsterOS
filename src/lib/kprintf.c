#include "aster/lib/kprintf.h"

#include <stdbool.h>

static void kprint_char(kprintf_putc_t putc, void *ctx, char c) {
    if (putc != 0) {
        putc(c, ctx);
    }
}

static void kprint_string(kprintf_putc_t putc, void *ctx, const char *str) {
    if (str == 0) {
        str = "(null)";
    }

    while (*str) {
        kprint_char(putc, ctx, *str++);
    }
}

static void kprint_u64_dec(kprintf_putc_t putc, void *ctx, uint64_t value) {
    char buffer[32];
    size_t index = 0;

    if (value == 0) {
        kprint_char(putc, ctx, '0');
        return;
    }

    while (value > 0) {
        buffer[index++] = (char)('0' + (value % 10));
        value /= 10;
    }

    while (index > 0) {
        kprint_char(putc, ctx, buffer[--index]);
    }
}

static void kprint_i64_dec(kprintf_putc_t putc, void *ctx, int64_t value) {
    if (value < 0) {
        kprint_char(putc, ctx, '-');

        /*
         * Avoid signed overflow on INT64_MIN.
         */
        uint64_t unsigned_value = (uint64_t)(-(value + 1)) + 1;
        kprint_u64_dec(putc, ctx, unsigned_value);
        return;
    }

    kprint_u64_dec(putc, ctx, (uint64_t)value);
}

static void kprint_u64_hex(
    kprintf_putc_t putc,
    void *ctx,
    uint64_t value,
    bool uppercase,
    bool prefix
) {
    const char *digits = uppercase
        ? "0123456789ABCDEF"
        : "0123456789abcdef";

    if (prefix) {
        kprint_string(putc, ctx, "0x");
    }

    bool started = false;

    for (int shift = 60; shift >= 0; shift -= 4) {
        uint8_t nibble = (uint8_t)((value >> shift) & 0xF);

        if (nibble != 0 || started || shift == 0) {
            started = true;
            kprint_char(putc, ctx, digits[nibble]);
        }
    }
}

static void kprint_padding(kprintf_putc_t putc, void *ctx, char pad, int count) {
    for (int i = 0; i < count; i++) {
        kprint_char(putc, ctx, pad);
    }
}

static size_t kstrlen(const char *str) {
    size_t len = 0;

    if (str == 0) {
        return 6; /* "(null)" */
    }

    while (str[len]) {
        len++;
    }

    return len;
}

static size_t u64_dec_len(uint64_t value) {
    size_t len = 1;

    while (value >= 10) {
        value /= 10;
        len++;
    }

    return len;
}

static size_t i64_dec_len(int64_t value) {
    if (value < 0) {
        uint64_t unsigned_value = (uint64_t)(-(value + 1)) + 1;
        return 1 + u64_dec_len(unsigned_value);
    }

    return u64_dec_len((uint64_t)value);
}

static size_t u64_hex_len(uint64_t value, bool prefix) {
    size_t len = prefix ? 2 : 0;

    bool started = false;

    for (int shift = 60; shift >= 0; shift -= 4) {
        uint8_t nibble = (uint8_t)((value >> shift) & 0xF);

        if (nibble != 0 || started || shift == 0) {
            started = true;
            len++;
        }
    }

    return len;
}

void kvprintf(kprintf_putc_t putc, void *ctx, const char *fmt, va_list args) {
    if (fmt == 0) {
        return;
    }

    while (*fmt) {
        if (*fmt != '%') {
            kprint_char(putc, ctx, *fmt++);
            continue;
        }

        fmt++;

        if (*fmt == '%') {
            kprint_char(putc, ctx, '%');
            fmt++;
            continue;
        }

        bool zero_pad = false;
        bool long_long = false;
        int width = 0;

        if (*fmt == '0') {
            zero_pad = true;
            fmt++;
        }

        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        if (*fmt == 'l') {
            fmt++;

            if (*fmt == 'l') {
                long_long = true;
                fmt++;
            }
        }

        char spec = *fmt;

        if (spec == '\0') {
            break;
        }

        switch (spec) {
            case 's': {
                const char *str = va_arg(args, const char *);
                size_t len = kstrlen(str);

                if (width > (int)len) {
                    kprint_padding(putc, ctx, ' ', width - (int)len);
                }

                kprint_string(putc, ctx, str);
                break;
            }

            case 'c': {
                char c = (char)va_arg(args, int);

                if (width > 1) {
                    kprint_padding(putc, ctx, ' ', width - 1);
                }

                kprint_char(putc, ctx, c);
                break;
            }

            case 'd':
            case 'i': {
                int64_t value;

                if (long_long) {
                    value = va_arg(args, int64_t);
                } else {
                    value = (int64_t)va_arg(args, int);
                }

                size_t len = i64_dec_len(value);

                if (width > (int)len) {
                    kprint_padding(putc, ctx, zero_pad ? '0' : ' ', width - (int)len);
                }

                kprint_i64_dec(putc, ctx, value);
                break;
            }

            case 'u': {
                uint64_t value;

                if (long_long) {
                    value = va_arg(args, uint64_t);
                } else {
                    value = (uint64_t)va_arg(args, unsigned int);
                }

                size_t len = u64_dec_len(value);

                if (width > (int)len) {
                    kprint_padding(putc, ctx, zero_pad ? '0' : ' ', width - (int)len);
                }

                kprint_u64_dec(putc, ctx, value);
                break;
            }

            case 'x':
            case 'X': {
                uint64_t value;

                if (long_long) {
                    value = va_arg(args, uint64_t);
                } else {
                    value = (uint64_t)va_arg(args, unsigned int);
                }

                size_t len = u64_hex_len(value, false);

                if (width > (int)len) {
                    kprint_padding(putc, ctx, zero_pad ? '0' : ' ', width - (int)len);
                }

                kprint_u64_hex(putc, ctx, value, spec == 'X', false);
                break;
            }

            case 'p': {
                uint64_t value = (uint64_t)va_arg(args, void *);
                size_t len = u64_hex_len(value, true);

                if (width > (int)len) {
                    kprint_padding(putc, ctx, zero_pad ? '0' : ' ', width - (int)len);
                }

                kprint_u64_hex(putc, ctx, value, false, true);
                break;
            }

            default:
                kprint_char(putc, ctx, '%');
                kprint_char(putc, ctx, spec);
                break;
        }

        fmt++;
    }
}

void kprintf_to(kprintf_putc_t putc, void *ctx, const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    kvprintf(putc, ctx, fmt, args);
    va_end(args);
}
