#ifndef CONSOLE_H
#define CONSOLE_H

// Put a character and string
#include <stdint.h>
void console_draw_char(char c, uint64_t x, uint64_t y, uint32_t fg, uint32_t bg);
void console_draw_string(const char *str, uint64_t x, uint64_t y, uint32_t fg, uint32_t bg);

#endif
