#include "cstdlib.h"
int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(unsigned char*)s1 - *(unsigned char*)s2;
}

void memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char*)dest;
    const unsigned char *s = (const unsigned char*)src;

    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

/**
 *  <0 symbol s1 < s2
 *  =0 symbol s1 == s2
 *  >0 symbol s1 > s2
 */
int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = (const unsigned char*)s1;
    const unsigned char *p2 = (const unsigned char*)s2;

    while (n-- > 0) {
        if (*p1 != *p2) {
            return (*p1) - (*p2);
        }
        p1++;
        p2++;
    }
    
    return 0;
}

bool memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char*)s;

    if (s == NULL || n == 0) {
        return false;
    }

    while (n--) {
        *p++ = (unsigned char)c;
    }

    return true;
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

char *strcpy(char *dest, const char *src) {
    char *d = dest;
    while ((*d++ = *src++) != '\0') {
        /* copy including the '\0' */
    }
    return dest;
}

char *strcat(char *dest, const char *src) {
    char *d = dest;

    while (*d) d++;

    while ((*d++ = *src++) != '\0') {
        /* append including the '\0' */
    }
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    char *d = dest;
    size_t i = 0;

    for (; i < n && src[i] != '\0'; ++i) {
        d[i] = src[i];
    }

    for (; i < n; ++i) {
        d[i] = '\0';
    }
    return dest;
}
