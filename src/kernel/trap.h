#ifndef __TRAP_H__
#define __TRAP_H__
#include "task.h"

// ================== synchronization exception（Highest bit=0）==================
#define MCAUSE_INSTR_ADDR_MISALIGN   0x0
#define MCAUSE_INSTR_ACCESS_FAULT   0x1
#define MCAUSE_ILLEGAL_INSTR        0x2
#define MCAUSE_BREAKPOINT           0x3
#define MCAUSE_LOAD_ADDR_MISALIGN   0x4
#define MCAUSE_LOAD_ACCESS_FAULT    0x5
#define MCAUSE_STORE_ADDR_MISALIGN  0x6
#define MCAUSE_STORE_ACCESS_FAULT   0x7
#define MCAUSE_ECALL_U_MODE         0x8
#define MCAUSE_ECALL_S_MODE         0x9
#define MCAUSE_ECALL_M_MODE         0xB
#define MCAUSE_INSTR_PAGE_FAULT     0xC
#define MCAUSE_LOAD_PAGE_FAULT      0xD
#define MCAUSE_STORE_PAGE_FAULT     0xF

// ================== asynchronous interrupt(Highest bit=1)==================
#define MCAUSE_MACHINE_SOFT_INT     0x8000000000000003
#define MCAUSE_MACHINE_TIMER_INT    0x8000000000000007
#define MCAUSE_MACHINE_EXT_INT      0x800000000000000B

#define MCAUSE_IS_INTERRUPT(cause)  ((cause) & 0x8000000000000000)

void handler_trap(struct context *ctx);

#endif /* __TRAP_H__ */