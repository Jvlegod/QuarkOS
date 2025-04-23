.equ USER_STACK_SIZE,  4096
.equ KERNEL_STACK_SIZE,  1024
.equ MAX_HARTS,   8 /* max hart num(TODO: support multiple harts) */

.section .text.init
.global _start

_start:
    csrr    a0, mhartid
    mv      s0, a0

    /* now we only support one hart(zero)*/
    bnez    a0, park_hart

    la      a1, _kernel_stack_top
    li      a2, KERNEL_STACK_SIZE
    li      a3, MAX_HARTS

    mul     a4, a0, a2 /* hart stack offset：hartid * STACK_SIZE */
    add     sp, a1, a4

    tail    start_kernel
park_hart:
    wfi
    j       park_hart

.extern start_kernel