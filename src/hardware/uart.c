#include "ktypes.h"
#include "platform.h"
#include "uart.h"
#include <stdarg.h> // impl by compiler

#define RING_BUF_SIZE 256
static volatile char rx_buf[RING_BUF_SIZE];
static volatile uint16_t rx_head = 0, rx_tail = 0;

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

void uart_isr() {
    while (uart_read_reg(LSR) & LSR_RX_READY) {
        rx_buf[rx_head++] = uart_read_reg(RHR);
        rx_head %= RING_BUF_SIZE;
    }
}

short uart_write_bulk(char *data, unsigned short len) {
    if (!data || len == 0) return 0;
    
    unsigned short sent = 0;
    while (sent < len) {
        if (uart_read_reg(LSR) & LSR_TX_IDLE) {
            uart_write_reg(THR, data[sent++]);
        }
    }
    return sent;
}

short uart_read_async(char *data, unsigned short len) {
    short count = 0;
    while (count < len && rx_tail != rx_head) {
        data[count++] = rx_buf[rx_tail++];
        rx_tail %= RING_BUF_SIZE;
    }
    return count;
}