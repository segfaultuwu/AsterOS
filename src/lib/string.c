#include <stddef.h>
#include <stdint.h>

void *memset(void *dest, int value, size_t count) {
    uint8_t *ptr = (uint8_t *)dest;

    for (size_t i = 0; i < count; i++) {
        ptr[i] = (uint8_t)value;
    }

    return dest;
}

void *memcpy(void *dest, const void *src, size_t count) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    for (size_t i = 0; i < count; i++) {
        d[i] = s[i];
    }

    return dest;
}

void *memmove(void *dest, const void *src, size_t count) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;

    if (d == s || count == 0) {
        return dest;
    }

    if (d < s) {
        for (size_t i = 0; i < count; i++) {
            d[i] = s[i];
        }
    } else {
        for (size_t i = count; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }

    return dest;
}

int memcmp(const void *a, const void *b, size_t count) {
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;

    for (size_t i = 0; i < count; i++) {
        if (pa[i] != pb[i]) {
            return pa[i] < pb[i] ? -1 : 1;
        }
    }

    return 0;
}

size_t strlen(const char *str) {
    size_t len = 0;

    while (str[len]) {
        len++;
    }

    return len;
}
