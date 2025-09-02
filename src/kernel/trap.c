#include "platform.h"
#include "interrupt.h"
#include "uart.h"
#include "trap.h"
#include "hwtimer.h"
#include "debug.h"
#include "syscall.h"

static void handle_illegal_instruction() {
    while(1) {
    }
}

static uint64_t handle_interrupt(uint64_t cause, uint64_t epc) {
    switch (cause) {
        case MCAUSE_MACHINE_TIMER_INT:
            handle_timer_interrupt();
            break;
        case MCAUSE_MACHINE_EXT_INT:
            handler_external_interrupt();
            break;
        default:
            LOG_ERROR("Unhandled interrupt: cause=%lx\r\n", cause);
            while(1);
    }
    return epc;
}
static uint64_t handle_exception(uint64_t cause, uint64_t epc, uint64_t mtval, struct context *ctx) {
    switch (cause) {
        case MCAUSE_ECALL_U_MODE:
            handle_syscall(ctx);
            epc += 4;
            break;
            
        case MCAUSE_ILLEGAL_INSTR:
            LOG_ERROR("Illegal instruction at 0x%lx: 0x%lx\r\n", epc, mtval);
            handle_illegal_instruction();
            break;
        case MCAUSE_INSTR_PAGE_FAULT:
            LOG_ERROR("Instruction page fault at 0x%lx\r\n", epc);
            while (1) { }
            break;
        case MCAUSE_LOAD_PAGE_FAULT:
            LOG_ERROR("Load page fault at 0x%lx (addr 0x%lx)\r\n", epc, mtval);
            while (1) { }
            break;
        case MCAUSE_STORE_PAGE_FAULT:
            LOG_ERROR("Store page fault at 0x%lx (addr 0x%lx)\r\n", epc, mtval);
            while (1) { }
            break;
        default:
            LOG_ERROR("Unhandled exception: cause=%lx\r\n", cause);
            while(1);
            break;
    }
    return epc;
}

// Do we really need all ctx?

void handler_trap(struct context *ctx) {
    uint64_t cause = read_csr(mcause);
    uint64_t epc = read_csr(mepc);
    uint64_t mtval = read_csr(mtval);
    uint64_t status = read_csr(mstatus);

    // type of interrupt
    if (MCAUSE_IS_INTERRUPT(cause)) {
        epc = handle_interrupt(cause, epc);
    } else {
        epc = handle_exception(cause, epc, mtval, ctx);
    }

    write_csr(mepc, epc);
    write_csr(mstatus, status);
}