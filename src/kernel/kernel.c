#include "ktypes.h"
#include "kprintf.h"
#include "mem.h"
#include "task.h"
#include "test.h"
#include "hwtimer.h"
#include "interrupt.h"
#include "lock.h"
void start_kernel(void)
{
	uart_init();
	uart_test();
	interrupt_init();
	timer_init();
	mem_init((uintptr_t)_heap_start, (uintptr_t)_heap_end);
	mem_test();
	// task init should after uart and mem.
	int hartid = read_tp();
	task_init(hartid);

	while (1);
}