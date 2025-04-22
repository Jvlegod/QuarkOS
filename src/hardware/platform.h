#ifndef __PLATFORM_H__
#define __PLATFORM_H__
#include "ktypes.h"
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

#define CLINT_BASE 0x2000000
#define MTIME (*(volatile uint64_t*)(CLINT_BASE + 0xBFF8))
#define MTIMECMP (*(volatile uint64_t*)(CLINT_BASE + 0x4000))

#define read_csr(csr) ({ unsigned long __v; __asm__ volatile ("csrr %0, " #csr : "=r"(__v)); __v; })
#define write_csr(csr, val) ({ __asm__ volatile ("csrw " #csr ", %0" :: "r"(val)); })

#endif /* __PLATFORM_H__ */
