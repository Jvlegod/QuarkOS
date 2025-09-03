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
#include "virtio_input.h"
#include "virtio_keyboard.h"
#include "virtio_tablet.h"
#include "virtio_net.h"
#include "lwip_port.h"
#include "fs.h"
#include "debug.h"
#include "user.h"
#include "vm.h"

void start_kernel(void)
{
	uart_init();
	uart_test();
	interrupt_init();
	timer_init();
	mem_init((uintptr_t)_heap_start, (uintptr_t)_heap_end);
	mem_test();
	vm_init();
	if (virtio_blk_init() != 0) {
        LOG_ERROR("[BOOT] virtio blk init failed!\r\n");
        while (1);
    }
	blk_test();
    uint64_t total_sectors = virtio_capacity_sectors();
    if (fs_mount_or_mkfs(total_sectors) != 0) {
        LOG_ERROR("[BOOT] fs mount_or_mkfs failed!\r\n");
        while (1);
    }
	fs_test();
    user_init();
    if (gfx_init() != 0) {
		LOG_ERROR("[BOOT] gfx(virtio gpu) init failed!\r\n");
        while (1);
    }
	gpu_test();
	input_init_all();

	if(virtio_net_init() != 0) {
		LOG_ERROR("[BOOT] virtio net init failed!\r\n");
	} else {
		lwip_port_init();
	}

	// task init should after uart and mem.
	int hartid = read_tp();
	task_init(hartid);

	while (1);
}