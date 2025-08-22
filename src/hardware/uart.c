#include "ktypes.h"
#include "platform.h"
#include "uart.h"
#include "cstdlib.h"
#include <stdarg.h> // impl by compiler

static struct uart_buf uart_rx_buf;

void uart_init(void)
{
    uart_write_reg(IER, 0x00);

    uart_write_reg(FCR, (1<<0) | (1<<1) | (1<<2));

    uint8_t lcr = uart_read_reg(LCR);
    uart_write_reg(LCR, lcr | (1 << 7));
    uart_write_reg(DLL, 0x01);
    uart_write_reg(DLM, 0x00);

    uart_write_reg(LCR, (3 << 0));

    uart_write_reg(MCR, 0x0B);

    (void)uart_read_reg(LSR);
    (void)uart_read_reg(RHR);
    (void)uart_read_reg(IIR);

    uart_write_reg(IER, 0x01);

    uart_rx_buf.rx_tail = 0;
    uart_rx_buf.rx_head = 0;
    memset((void*)uart_rx_buf.rx_buf, 0, sizeof(uart_rx_buf.rx_buf));
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

void uart_isr(void) {
    while (uart_read_reg(LSR) & LSR_RX_READY) {
        char c = uart_read_reg(RHR);

        if (c == 0x08 || c == 0x7F) {
            if (uart_rx_buf.rx_head != uart_rx_buf.rx_tail) {
                uart_rx_buf.rx_head = (uart_rx_buf.rx_head + RING_BUF_SIZE - 1) % RING_BUF_SIZE;
                if (uart_read_reg(LSR) & LSR_TX_IDLE) { uart_write_reg(THR, '\b'); }
                if (uart_read_reg(LSR) & LSR_TX_IDLE) { uart_write_reg(THR, ' '); }
                if (uart_read_reg(LSR) & LSR_TX_IDLE) { uart_write_reg(THR, '\b'); }
            } else {
                if (uart_read_reg(LSR) & LSR_TX_IDLE) uart_write_reg(THR, '\a');
            }
            continue;
        }

        uint16_t next = (uart_rx_buf.rx_head + 1) % RING_BUF_SIZE;
        if (next != uart_rx_buf.rx_tail) {
            uart_rx_buf.rx_buf[uart_rx_buf.rx_head] = c;
            barrier();
            uart_rx_buf.rx_head = next;
            if (uart_read_reg(LSR) & LSR_TX_IDLE) uart_write_reg(THR, c);
        } else {
            if (uart_read_reg(LSR) & LSR_TX_IDLE) uart_write_reg(THR, '\a');
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

bool shell_uart_fflush(char *buf) {
	int i = 0;
	while (!shell_if_fflush());
	uart_putc('\n');
	while (uart_rx_buf.rx_head != uart_rx_buf.rx_tail) {
		buf[i++] = uart_rx_buf.rx_buf[uart_rx_buf.rx_tail]; // TODO: buf may out of range
		uart_rx_buf.rx_buf[uart_rx_buf.rx_tail] = 0;
		uart_rx_buf.rx_tail = (uart_rx_buf.rx_tail + 1) % RING_BUF_SIZE;
	}
    return true;
}