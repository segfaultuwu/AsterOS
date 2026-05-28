#ifndef ASTER_DEBUG_LOGGING_H
#define ASTER_DEBUG_LOGGING_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef enum {
    LOG_LEVEL_INFO,
    LOG_LEVEL_OK,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,
    LOG_LEVEL_PANIC,
} log_level_t;

void log_init(void);

void log_write(log_level_t level, const char *message);

/* printf-style logging */
void log_printf(log_level_t level, const char *fmt, ...);

void log_info(const char *message);
void log_ok(const char *message);
void log_warn(const char *message);
void log_error(const char *message);
void log_panic(const char *message);

/* printf-style helpers */
void log_infof(const char *fmt, ...);
void log_okf(const char *fmt, ...);
void log_warnf(const char *fmt, ...);
void log_errorf(const char *fmt, ...);
void log_panicf(const char *fmt, ...);

void log_hex64(const char *label, uint64_t value);
void log_u64(const char *label, uint64_t value);

#endif
