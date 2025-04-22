#ifndef __UART_H__
#define __UART_H__

// NS16550A
void uart_init();
void uart_puts(char *s);
int uart_putc(char ch);

#endif /* __UART_H__ */