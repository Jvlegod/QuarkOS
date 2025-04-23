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

#define PRINT_BUF_SIZE 32

// NS16550A
void uart_init();
void uart_printf(const char *fmt, ...);

#endif /* __UART_H__ */