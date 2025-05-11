#ifndef __PLATFORM_H__
#define __PLATFORM_H__
#include "ktypes.h"
#include "regs.h"
#define MAXNUM_CPU 8

/*
 * MemoryMap
 * see https://github.com/qemu/qemu/blob/master/hw/riscv/virt.c, virt_memmap[]
 * 0x00001000 -- boot ROM, provided by qemu
 * 0x02000000 -- CLINT
 * 0x0C000000 -- PLIC
 * 0x10000000 -- UART0
 * 0x10001000 -- virtio disk
 * 0x80000000 -- boot ROM jumps here in machine mode, where we load our kernel
 */

#define UART0 0x10000000L
#define CLINT_BASE 0x2000000L
#define PLIC_BASE 0x0C000000L

// CLINT
#define MTIME (*(volatile uint64_t*)(CLINT_BASE + 0xBFF8))
#define MTIMECMP(hartid) (*(volatile uint64_t*)(CLINT_BASE + 0x4000 + 8 * hartid))

// PLIC
#define PLIC_PRIORITY(id) (PLIC_BASE + (id) * 4)
#define PLIC_PENDING(id) (PLIC_BASE + 0x1000 + ((id) / 32) * 4)
#define PLIC_MENABLE(hart, id) (PLIC_BASE + 0x2000 + (hart) * 0x80 + ((id) / 32) * 4)
#define PLIC_MTHRESHOLD(hart) (PLIC_BASE + 0x200000 + (hart) * 0x1000)
#define PLIC_MCLAIM(hart) (PLIC_BASE + 0x200004 + (hart) * 0x1000)
#define PLIC_MCOMPLETE(hart) (PLIC_BASE + 0x200004 + (hart) * 0x1000)

#endif /* __PLATFORM_H__ */
