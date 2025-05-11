.section .text

.macro reg_save base
    sd ra,  0 * 8(\base)  # x1
    sd sp,  1 * 8(\base)  # x2
    sd t0,  2 * 8(\base)  # x5
    sd t1,  3 * 8(\base)  # x6
    sd t2,  4 * 8(\base)  # x7
    sd a0,  5 * 8(\base)  # x10
    sd a1,  6 * 8(\base)  # x11
    sd a2,  7 * 8(\base)  # x12
    sd a3,  8 * 8(\base)  # x13
    sd a4,  9 * 8(\base)  # x14
    sd a5, 10 * 8(\base)  # x15
    sd a6, 11 * 8(\base)  # x16
    sd a7, 12 * 8(\base)  # x17
    sd t3, 13 * 8(\base)  # x28
    sd t4, 14 * 8(\base)  # x29
    sd t5, 15 * 8(\base)  # x30
    sd t6, 16 * 8(\base)  # x31
    sd s0, 17 * 8(\base)  # x8
    sd s1, 18 * 8(\base)  # x9
    sd s2, 19 * 8(\base)  # x18
    sd s3, 20 * 8(\base)  # x19
    sd s4, 21 * 8(\base)  # x20
    sd s5, 22 * 8(\base)  # x21
    sd s6, 23 * 8(\base)  # x22
    sd s7, 24 * 8(\base)  # x23
    sd s8, 25 * 8(\base)  # x24
    sd s9, 26 * 8(\base)  # x25
    sd s10,27 * 8(\base)  # x26
    sd s11,28 * 8(\base)  # x27
    sd gp, 29 * 8(\base)  # x3
.endm

.macro reg_restore base
    ld ra,  0 * 8(\base)  # x1
    ld sp,  1 * 8(\base)  # x2
    ld t0,  2 * 8(\base)  # x5
    ld t1,  3 * 8(\base)  # x6
    ld t2,  4 * 8(\base)  # x7
    ld a0,  5 * 8(\base)  # x10
    ld a1,  6 * 8(\base)  # x11
    ld a2,  7 * 8(\base)  # x12
    ld a3,  8 * 8(\base)  # x13
    ld a4,  9 * 8(\base)  # x14
    ld a5, 10 * 8(\base)  # x15
    ld a6, 11 * 8(\base)  # x16
    ld a7, 12 * 8(\base)  # x17
    ld t3, 13 * 8(\base)  # x28
    ld t4, 14 * 8(\base)  # x29
    ld t5, 15 * 8(\base)  # x30
    ld t6, 16 * 8(\base)  # x31
    ld s0, 17 * 8(\base)  # x8
    ld s1, 18 * 8(\base)  # x9
    ld s2, 19 * 8(\base)  # x18
    ld s3, 20 * 8(\base)  # x19
    ld s4, 21 * 8(\base)  # x20
    ld s5, 22 * 8(\base)  # x21
    ld s6, 23 * 8(\base)  # x22
    ld s7, 24 * 8(\base)  # x23
    ld s8, 25 * 8(\base)  # x24
    ld s9, 26 * 8(\base)  # x25
    ld s10,27 * 8(\base)  # x26
    ld s11,28 * 8(\base)  # x27
    ld gp, 29 * 8(\base)  # x3
    ld tp, 30 * 8(\base)  # x4
.endm

.extern handler_trap
.global trap_entry
.align 4
# M mode
trap_entry:
    csrrw tp, mscratch, tp
    reg_save tp

    mv t1, tp
    csrr t0, mscratch
    sd t0, 30 * 8(t1)
    mv tp, t0

    csrr t0, mepc
    sd t0, 32 * 8(t1)
    csrw mscratch, t1

    csrr a0, mscratch  # arg1：struct context pointer
    call handler_trap

    csrr tp, mscratch
    reg_restore tp
    
    mret

# ctx_int_switch can only occur in interrupt
.global ctx_int_switch
.align 4
ctx_int_switch:
        csrw mscratch, a0

        ld ra,  0 * 8(a0)  # x1
        ld sp,  1 * 8(a0)  # x2
        ld t0,  2 * 8(a0)  # x5
        ld t1,  3 * 8(a0)  # x6
        ld t2,  4 * 8(a0)  # x7
        ld a1,  6 * 8(a0)  # x11
        ld a2,  7 * 8(a0)  # x12
        ld a3,  8 * 8(a0)  # x13
        ld a4,  9 * 8(a0)  # x14
        ld a5, 10 * 8(a0)  # x15
        ld a6, 11 * 8(a0)  # x16
        ld a7, 12 * 8(a0)  # x17
        ld t3, 13 * 8(a0)  # x28
        ld t4, 14 * 8(a0)  # x29
        ld t5, 15 * 8(a0)  # x30
        ld t6, 16 * 8(a0)  # x31
        ld s0, 17 * 8(a0)  # x8
        ld s1, 18 * 8(a0)  # x9
        ld s2, 19 * 8(a0)  # x18
        ld s3, 20 * 8(a0)  # x19
        ld s4, 21 * 8(a0)  # x20
        ld s5, 22 * 8(a0)  # x21
        ld s6, 23 * 8(a0)  # x22
        ld s7, 24 * 8(a0)  # x23
        ld s8, 25 * 8(a0)  # x24
        ld s9, 26 * 8(a0)  # x25
        ld s10,27 * 8(a0)  # x26
        ld s11,28 * 8(a0)  # x27
        ld gp, 29 * 8(a0)  # x3
        ld tp, 30 * 8(a0)  # x4

        ld t0, 31 * 8(a0)
        csrw mstatus, t0
        ld t0, 32 * 8(a0)
        csrw mepc, t0
        
        # ld a0,  5 * 8(a0)  # x10
        
        mret

# ----------------------------
# void ctx_switch(struct context *old, struct context *new);
# a0: old context
# a1: new context
# ----------------------------
.global ctx_switch
.align 4
ctx_switch:
        sd ra, 0 * 8(a0)
        sd sp, 1 * 8(a0)
        sd s0, 17 * 8(a0)
        sd s1, 18 * 8(a0)
        sd s2, 19 * 8(a0)
        sd s3, 20 * 8(a0)
        sd s4, 21 * 8(a0)
        sd s5, 22 * 8(a0)
        sd s6, 23 * 8(a0)
        sd s7, 24 * 8(a0)
        sd s8, 25 * 8(a0)
        sd s9, 26 * 8(a0)
        sd s10, 27 * 8(a0)
        sd s11, 28 * 8(a0)
        
        csrr t0, mstatus
        sd t0, 31 * 8(a0)
        csrr t0, mepc
        sd t0, 32 * 8(a0)

        ld ra, 0 * 8(a1)
        ld sp, 1 * 8(a1)
        ld s0, 17 * 8(a1)
        ld s1, 18 * 8(a1)
        ld s2, 19 * 8(a1)
        ld s3, 20 * 8(a1)
        ld s4, 21 * 8(a1)
        ld s5, 22 * 8(a1)
        ld s6, 23 * 8(a1)
        ld s7, 24 * 8(a1)
        ld s8, 25 * 8(a1)
        ld s9, 26 * 8(a1)
        ld s10, 27 * 8(a1)
        ld s11, 28 * 8(a1)

        ld t0, 31 * 8(a1)
        csrw mstatus, t0
        ld t0, 32 * 8(a1)
        csrw mepc, t0
        
        mret