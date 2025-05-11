#ifndef __PRINTF_H__
#define __PRINTF_H__
#include "uart.h"

// in QEMU -serial mon:stdio 'ENTER' on keyboard will be deal with '\r'
void kprintf(const char *fmt, ...);

#endif /* __PRINTF_H__ */