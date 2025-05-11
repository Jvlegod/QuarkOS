#include "kprintf.h"
#include <stdarg.h>

static void print_num_long(unsigned long num, int base) {
    char buffer[65];
    char *ptr = buffer + sizeof(buffer) - 1;
    *ptr = '\0';

    do {
        *--ptr = "0123456789abcdef"[num % base];
        num /= base;
    } while (num != 0);

    uart_puts(ptr);
}

static void print_num(unsigned long num, int base) {
    char buf[PRINT_BUF_SIZE];
    char *ptr = &buf[PRINT_BUF_SIZE - 1];
    *ptr = '\0';

    const char digits[] = "0123456789abcdef";
    
    do {
        *--ptr = digits[num % base];
        num /= base;
    } while (num != 0);

    uart_puts(ptr);
}

void kprintf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            char c = *fmt;
            if (c == '\r') {
                uart_putc('\n');
            }
            uart_putc(c);
            continue;
        }

        fmt++;
        switch (*fmt) {
            case 'd': {
                int num = va_arg(args, int);
                if (num < 0) {
                    uart_putc('-');
                    num = -num;
                }
                print_num(num, 10);
                break;
            }
            case 'u':
                print_num(va_arg(args, unsigned int), 10);
                break;
            case 'l': {
                fmt++;
                unsigned long num = va_arg(args, unsigned long);
                if (*fmt == 'u') {
                    print_num_long(num, 10);
                } else if (*fmt == 'x') {
                    print_num_long(num, 16);
                } else {
                    uart_puts("%l?");
                }
                break;
            }
            case 'x':
                print_num(va_arg(args, unsigned int), 16);
                break;
            case 'c': {
                char c = (char)va_arg(args, int);
                if (c == '\r') {
                    uart_putc('\n');
                }
                uart_putc(c);
                break;
            }
            case 's': {
                char *str = va_arg(args, char*);
                if (!str) str = "(null)";
                for (char *p = str; *p; p++) {
                    if (*p == '\r') {
                        uart_putc('\n');
                    }
                    uart_putc(*p);
                }
                break;
            }
            case '%':
                uart_putc('%');
                break;
            default:
                uart_putc('?');
                break;
        }
    }

    va_end(args);
}