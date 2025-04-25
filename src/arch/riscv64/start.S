/* Now our cpu only work in M and U Mode */
.equ KERNEL_STACK_SIZE,  1024
.equ MAX_HARTS,   8 /* max hart num(TODO: support multiple harts) */

.section .text.init
.global _start

_start:
    csrr    a0, mhartid
    mv      tp, a0

    /* now we only support one hart(zero)*/
    bnez    a0, park_hart

    la      a1, _kernel_stack_top
    li      a2, KERNEL_STACK_SIZE
    li      a3, MAX_HARTS

    mul     a4, a0, a2 /* kernel hart stack offset：hartid * STACK_SIZE */
    add     sp, a1, a4

    tail    start_kernel
park_hart:
    wfi
    j       park_hart

.extern start_kernel

/* only call on the fist, just switch to the idle task */
.global start_switch_to
.align 4
start_switch_to:
        ld ra, 0 * 8(a0)
        ld sp, 1 * 8(a0)
        ld s0, 17 * 8(a0)
        ld s1, 18 * 8(a0)
        ld s2, 19 * 8(a0)
        ld s3, 20 * 8(a0)
        ld s4, 21 * 8(a0)
        ld s5, 22 * 8(a0)
        ld s6, 23 * 8(a0)
        ld s7, 24 * 8(a0)
        ld s8, 25 * 8(a0)
        ld s9, 26 * 8(a0)
        ld s10, 27 * 8(a0)
        ld s11, 28 * 8(a0)

        ld t0, 31 * 8(a0)
        csrw mstatus, t0
        ld t0, 32 * 8(a0)
        csrw mepc, t0
        
        mret