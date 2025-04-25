#include "platform.h"
#include "interrupt.h"
#include "uart.h"
#include "trap.h"
#include "hwtimer.h"
#include "kprintf.h"

static void handle_syscall() {
    while(1) {

    }
}

static void handle_illegal_instruction() {
    while(1) {

    }
}
static void handle_interrupt(uint64_t cause, uint64_t epc) {
    switch (cause) {
        case MCAUSE_MACHINE_TIMER_INT:
            handle_timer_interrupt();
            break;
        case MCAUSE_MACHINE_EXT_INT:
            handler_external_interrupt();
            break;
        default:
            kprintf("Unhandled interrupt: cause=%lx\n", cause);
            while(1);
    }
}
static void handle_exception(uint64_t cause, uint64_t epc, uint64_t mtval) {
    switch (cause) {
        case MCAUSE_ECALL_U_MODE:
            uint64_t ext_id = 0; // TODO?
            uint64_t func_id = 0; // TODO?
            handle_syscall(ext_id, func_id);
            break;
            
        case MCAUSE_ILLEGAL_INSTR:
            kprintf("Illegal instruction at 0x%lx: 0x%lx\n", epc, mtval);
            handle_illegal_instruction();
            break;
            
        default:
            kprintf("Unhandled exception: cause=%lx\n", cause);
            while(1);
            break;
    }
}

// Do we really need all ctx?

void handler_trap(struct context *ctx) {
    uint64_t cause = read_csr(mcause);
    uint64_t epc = read_csr(mepc);
    uint64_t mtval = read_csr(mtval);
    uint64_t status = read_csr(mstatus);

    // type of interrupt
    if (MCAUSE_IS_INTERRUPT(cause)) {
        handle_interrupt(cause, epc);
    } else {
        handle_exception(cause, epc, mtval);
    }

    write_csr(mepc, epc);
    write_csr(mstatus, status);
}