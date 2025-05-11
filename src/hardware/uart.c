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
    uart_write_reg(IER, uart_read_reg(IER) | (1 << 0));
}
void uart_putc(char ch) {
    while ((uart_read_reg(LSR) & LSR_TX_IDLE) == 0);
    uart_write_reg(THR, ch);
}

int uart_getc() {
    if (uart_read_reg(LSR) & LSR_RX_READY) {
        return uart_read_reg(RHR);
    } else {
        return -1;
    }
}

void uart_puts(char *s) {
	while (*s) {
		uart_putc(*s++);
	}
}