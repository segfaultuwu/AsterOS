#ifndef ASTER_STRING_H
#define ASTER_STRING_H

#include <stddef.h>
#include <stdint.h>

void *memset(void *dest, int value, size_t count);
void *memcpy(void *dest, const void *src, size_t count);
void *memmove(void *dest, const void *src, size_t count);
int memcmp(const void *a, const void *b, size_t count);

size_t strlen(const char *str);

#endif
