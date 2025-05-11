#ifndef __UART_H__
#define __UART_H__

// http://byterunner.com/16550.html

#define UART_REG(reg) ((volatile uint8_t *)(UART0 + reg))

#define RHR 0
#define THR 0
#define DLL 0
#define IER 1
#define DLM 1
#define FCR 2
#define ISR 2
#define LCR 3
#define MCR 4
#define LSR 5
#define MSR 6
#define SPR 7

#define LSR_RX_READY (1 << 0)
#define LSR_TX_IDLE  (1 << 5)

#define uart_read_reg(reg) (*(UART_REG(reg)))
#define uart_write_reg(reg, v) (*(UART_REG(reg)) = (v))

#define PRINT_BUF_SIZE 65

// NS16550A
void uart_init();
int uart_getc();
void uart_putc(char ch);
void uart_puts(char *s);
short uart_write_bulk(char *data, unsigned short len);
short uart_read_async(char *data, unsigned short len);
void uart_isr();

#endif /* __UART_H__ */