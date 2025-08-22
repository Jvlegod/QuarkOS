#include "ktypes.h"
#include "kprintf.h"
#include "mem.h"
#include "task.h"
#include "test.h"
#include "hwtimer.h"
#include "interrupt.h"
#include "lock.h"
#include "virtio_blk.h"
#include "gfx.h" /* #include "virtio_gpu.h" */
#include "fs.h"

void start_kernel(void)
{
	uart_init();
	uart_test();
	interrupt_init();
	timer_init();
	mem_init((uintptr_t)_heap_start, (uintptr_t)_heap_end);
	mem_test();
	if (virtio_blk_init() != 0) {
        kprintf("[BOOT] virtio blk init failed!\r\n");
        while (1);
    }
	blk_test();
    uint64_t total_sectors = virtio_capacity_sectors();
    if (fs_mount_or_mkfs(total_sectors) != 0) {
        kprintf("[BOOT] fs mount_or_mkfs failed!\r\n");
        while (1);
    }
	fs_test();
    if (gfx_init() != 0) {
		kprintf("[BOOT] gfx(virtio gpu) init failed!\r\n");
        while (1);
    }
	gpu_test();
	// task init should after uart and mem.
	int hartid = read_tp();
	task_init(hartid);

	while (1);
}