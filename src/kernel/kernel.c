#include "ktypes.h"
#include "uart.h"
#include "mem.h"
#include "test.h"
void start_kernel(void)
{
	uart_init();
	uart_test();
	mem_init((uintptr_t)_heap_start, (uintptr_t)_heap_end);
	mem_test();
	uart_puts("Hello, QuarkOS!\n");

	while (1) {};
}