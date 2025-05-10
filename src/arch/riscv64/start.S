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

    /* for qemu */
    li      t0, 0xffffffff
	csrw    pmpaddr0, t0
	li      t0, 0xf
	csrw    pmpcfg0, t0

    tail    start_kernel
park_hart:
    wfi
    j       park_hart

.extern start_kernel