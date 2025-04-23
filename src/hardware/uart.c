#include "ktypes.h"
#include "platform.h"
#include "uart.h"
#include <stdarg.h> // impl by compiler

void uart_init()
{
	/* disable interrupts. */
	uart_write_reg(IER, 0x00);

	uint8_t lcr = uart_read_reg(LCR);
	uart_write_reg(LCR, lcr | (1 << 7));
	uart_write_reg(DLL, 0x03);
	uart_write_reg(DLM, 0x00);

	lcr = 0;
	uart_write_reg(LCR, lcr | (3 << 0));
}
static void uart_putc(char ch) {
    while ((uart_read_reg(LSR) & LSR_TX_IDLE) == 0);
    uart_write_reg(THR, ch);
}

static void uart_puts(char *s)
{
	while (*s) {
		uart_putc(*s++);
	}
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

void uart_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    for (; *fmt; fmt++) {
        if (*fmt != '%') {
            uart_putc(*fmt);
            continue;
        }

        fmt++;  // skip '%'
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
            case 'x':
                print_num(va_arg(args, unsigned int), 16);
                break;
            case 'c':
                uart_putc((char)va_arg(args, int));
                break;
            case 's': {
                char *str = va_arg(args, char*);
                uart_puts(str ? str : "(null)");
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