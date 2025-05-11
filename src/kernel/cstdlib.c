#include "cstdlib.h"
int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

int memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char*)s;

    if (s == NULL || n == 0) {
        return 0;
    }

    while (n--) {
        *p++ = (unsigned char)c;
    }

    return 1;
}

size_t strlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return p - s;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    for (; n && *s1 && *s2; n--, s1++, s2++) {
        if (*s1 != *s2) return *s1 - *s2;
    }
    return n ? *s1 - *s2 : 0;
}