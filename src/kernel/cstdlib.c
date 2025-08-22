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

static void _putc_counted(char c, char* buf, size_t size, size_t* idx, int* total) {
    if (*total >= 0) (*total)++;
    if (size > 0 && *idx + 1 < size) {
        buf[*idx] = c;
        (*idx)++;
    }
}

static void _pad(char ch, int count, char* buf, size_t size, size_t* idx, int* total) {
    for (int i = 0; i < count; ++i) _putc_counted(ch, buf, size, idx, total);
}

static const char* _ull_to_str(unsigned long long v, unsigned base, int upper,
                               char* tmp_end, int* out_len, int precision_zero_is_empty) {
    static const char* digs_l = "0123456789abcdefghijklmnopqrstuvwxyz";
    static const char* digs_u = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const char* digs = upper ? digs_u : digs_l;

    char* p = tmp_end;
    int len = 0;

    if (v == 0) {
        if (precision_zero_is_empty) { *out_len = 0; return p; }
        *--p = '0';
        len = 1;
    } else {
        while (v) {
            *--p = digs[v % base];
            v /= base;
            len++;
        }
    }

    *out_len = len;
    return p;
}

int vsnprintf(char* buf, size_t size, const char* fmt, va_list ap) {
    size_t idx = 0;
    int total = 0;
    if (!buf || !fmt) return -1;

    for (const char* s = fmt; *s; ++s) {
        if (*s != '%') { _putc_counted(*s, buf, size, &idx, &total); continue; }

        // "%%"
        ++s;
        if (*s == '%') { _putc_counted('%', buf, size, &idx, &total); continue; }

        // flags
        int left = 0, zero = 0, hash = 0;
        for (;; ++s) {
            if (*s == '-') left = 1;
            else if (*s == '0') zero = 1;
            else if (*s == '#') hash = 1;
            else break;
        }

        // width
        int width = 0;
        while (is_digit(*s)) { width = width*10 + (*s - '0'); ++s; }

        // precision
        int prec = -1;
        if (*s == '.') {
            ++s; prec = 0;
            while (is_digit(*s)) { prec = prec*10 + (*s - '0'); ++s; }
        }

        // length
        int lenmod = 0;
        if (*s == 'l') { lenmod = 1; ++s; if (*s == 'l') { lenmod = 2; ++s; } }

        char spec = *s;
        if (!spec) break;

        if (spec == 's') {
            const char* str = va_arg(ap, const char*);
            if (!str) str = "(null)";
            int slen = (int)strlen(str);
            if (prec >= 0 && slen > prec) slen = prec;

            int pad = (width > slen) ? (width - slen) : 0;
            if (!left) _pad(' ', pad, buf, size, &idx, &total);
            for (int i = 0; i < slen; ++i) _putc_counted(str[i], buf, size, &idx, &total);
            if (left) _pad(' ', pad, buf, size, &idx, &total);
            continue;
        }

        if (spec == 'c') {
            int ch = va_arg(ap, int);
            int pad = (width > 1) ? (width - 1) : 0;
            if (!left) _pad(' ', pad, buf, size, &idx, &total);
            _putc_counted((char)ch, buf, size, &idx, &total);
            if (left) _pad(' ', pad, buf, size, &idx, &total);
            continue;
        }

        if (spec == 'd' || spec == 'i' || spec == 'u' || spec == 'x' || spec == 'X' || spec == 'p') {
            int is_signed = (spec == 'd' || spec == 'i');
            int base = (spec == 'x' || spec == 'X' || spec == 'p') ? 16 : 10;
            int upper = (spec == 'X');
            unsigned long long uval = 0;
            int negative = 0;

            if (spec == 'p') {
                uintptr_t pv = (uintptr_t)va_arg(ap, void*);
                uval = (unsigned long long)pv;
                hash = 1;
            } else if (is_signed) {
                long long sval;
                if (lenmod == 2)      sval = va_arg(ap, long long);
                else if (lenmod == 1) sval = va_arg(ap, long);
                else                  sval = va_arg(ap, int);
                if (sval < 0) { negative = 1; uval = (unsigned long long)(-sval); }
                else uval = (unsigned long long)sval;
            } else {
                if (lenmod == 2)      uval = va_arg(ap, unsigned long long);
                else if (lenmod == 1) uval = va_arg(ap, unsigned long);
                else                  uval = va_arg(ap, unsigned int);
            }

            char tmp[64];
            char* end = tmp + sizeof(tmp);
            int digits_len = 0;

            int precision_zero_is_empty = (prec == 0);
            const char* num = _ull_to_str(uval, (unsigned)base, upper, end, &digits_len, precision_zero_is_empty);

            char prefix[3]; int prefix_len = 0;
            if (negative) prefix[prefix_len++] = '-';
            else (void)0;

            if (hash && (spec == 'x' || spec == 'X' || spec == 'p')) {
                if (spec == 'p' || uval != 0) {
                    prefix[prefix_len++] = '0';
                    prefix[prefix_len++] = (upper ? 'X' : 'x');
                }
            }

            int prec_zeros = 0;
            if (prec >= 0) {
                if (digits_len < prec) prec_zeros = prec - digits_len;
                zero = 0;
            } else if (zero && !left && width > (prefix_len + digits_len)) {
                prec_zeros = width - prefix_len - digits_len;
            }

            int content_len = prefix_len + prec_zeros + digits_len;
            int pad_spaces = (width > content_len) ? (width - content_len) : 0;

            if (!left) _pad(' ', pad_spaces, buf, size, &idx, &total);

            for (int i = 0; i < prefix_len; ++i) _putc_counted(prefix[i], buf, size, &idx, &total);
            _pad('0', prec_zeros, buf, size, &idx, &total);
            for (int i = 0; i < digits_len; ++i) _putc_counted(num[i], buf, size, &idx, &total);

            if (left) _pad(' ', pad_spaces, buf, size, &idx, &total);
            continue;
        }

        _putc_counted('%', buf, size, &idx, &total);
        _putc_counted(spec, buf, size, &idx, &total);
    }

    if (size > 0) {
        if (idx >= size) buf[size - 1] = '\0';
        else buf[idx] = '\0';
    }
    return total;
}

int snprintf(char* buf, size_t size, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, size, fmt, ap);
    va_end(ap);
    return n;
}

int is_digit(char c) {
    return (c >= '0' && c <= '9');
}