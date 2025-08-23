#include "platform.h"
#include "interrupt.h"
#include "uart.h"
#include "trap.h"
#include "hwtimer.h"
#include "debug.h"

/*
 * a7: syscall number
 * a0~a6: parameters
 */
static void handle_syscall() {
    while(1) {
    }
}

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
static uint64_t handle_exception(uint64_t cause, uint64_t epc, uint64_t mtval) {
    switch (cause) {
        case MCAUSE_ECALL_U_MODE:
            uint64_t ext_id = 0; // TODO?
            uint64_t func_id = 0; // TODO?
            handle_syscall(ext_id, func_id);
            epc += 4;
            break;
            
        case MCAUSE_ILLEGAL_INSTR:
            LOG_ERROR("Illegal instruction at 0x%lx: 0x%lx\r\n", epc, mtval);
            handle_illegal_instruction();
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
        epc = handle_exception(cause, epc, mtval);
    }

    write_csr(mepc, epc);
    write_csr(mstatus, status);
}