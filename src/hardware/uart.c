#include "ktypes.h"
#include "platform.h"
#include "uart.h"
#include "cstdlib.h"
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

	uart_rx_buf.rx_tail = 0;
	uart_rx_buf.rx_head = 0;
	memset(uart_rx_buf.rx_buf, 0, sizeof(RING_BUF_SIZE));
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

void uart_isr() {
    while (uart_read_reg(LSR) & LSR_RX_READY) {
        char c = uart_read_reg(RHR);
        bool is_backspace = (c == 0x08 || c == 0x7F);

        if (is_backspace) {
            if (uart_rx_buf.rx_head != uart_rx_buf.rx_tail) {
                uart_rx_buf.rx_head = (uart_rx_buf.rx_head - 1 + RING_BUF_SIZE) % RING_BUF_SIZE;
                uart_putc('\x08');
                uart_putc(' ');
                uart_putc('\x08');
            } else {
                uart_putc('\a');
            }
        } else {
            uint16_t next_head = (uart_rx_buf.rx_head + 1) % RING_BUF_SIZE;
            if (next_head != uart_rx_buf.rx_tail) {
                uart_rx_buf.rx_buf[uart_rx_buf.rx_head] = c;
                uart_rx_buf.rx_head = next_head;
                uart_putc(c);
            } else {
                uart_putc('\a');
            }
        }
    }
}

bool shell_if_fflush() {
    int i = uart_rx_buf.rx_tail;
    char *buf = uart_rx_buf.rx_buf;
    while (i != uart_rx_buf.rx_head) {
        if (buf[i] == '\r' || buf[i] == '\n') {
            return true;
        }
        i = (i + 1) % RING_BUF_SIZE;
    }
    return false;
}
int shell_uart_fflush(char *buf) {
	int i = 0;
	while (!shell_if_fflush());
	uart_putc('\n');
	while (uart_rx_buf.rx_head != uart_rx_buf.rx_tail) {
		buf[i++] = uart_rx_buf.rx_buf[uart_rx_buf.rx_tail]; // TODO: buf may out of range
		uart_rx_buf.rx_buf[uart_rx_buf.rx_tail] = 0;
		uart_rx_buf.rx_tail = (uart_rx_buf.rx_tail + 1) % RING_BUF_SIZE;
	}
}