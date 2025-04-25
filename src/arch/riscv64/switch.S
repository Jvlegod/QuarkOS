.section .text
.extern handler_trap
.global trap_entry
.align 4
# M mode
trap_entry:
    addi sp, sp, -31 * 8
    sd ra,  0 * 8(sp)  # x1
    sd sp,  1 * 8(sp)  # x2
    sd t0,  2 * 8(sp)  # x5
    sd t1,  3 * 8(sp)  # x6
    sd t2,  4 * 8(sp)  # x7
    sd a0,  5 * 8(sp)  # x10
    sd a1,  6 * 8(sp)  # x11
    sd a2,  7 * 8(sp)  # x12
    sd a3,  8 * 8(sp)  # x13
    sd a4,  9 * 8(sp)  # x14
    sd a5, 10 * 8(sp)  # x15
    sd a6, 11 * 8(sp)  # x16
    sd a7, 12 * 8(sp)  # x17
    sd t3, 13 * 8(sp)  # x28
    sd t4, 14 * 8(sp)  # x29
    sd t5, 15 * 8(sp)  # x30
    sd t6, 16 * 8(sp)  # x31
    sd s0, 17 * 8(sp)  # x8
    sd s1, 18 * 8(sp)  # x9
    sd s2, 19 * 8(sp)  # x18
    sd s3, 20 * 8(sp)  # x19
    sd s4, 21 * 8(sp)  # x20
    sd s5, 22 * 8(sp)  # x21
    sd s6, 23 * 8(sp)  # x22
    sd s7, 24 * 8(sp)  # x23
    sd s8, 25 * 8(sp)  # x24
    sd s9, 26 * 8(sp)  # x25
    sd s10,27 * 8(sp)  # x26
    sd s11,28 * 8(sp)  # x27
    sd gp, 29 * 8(sp)  # x3
    sd tp, 30 * 8(sp)  # x4

    mv a0, sp          # arg1：struct context pointer
    call handler_trap  # TODO: here we will change to handler_trap

    ld ra,  0 * 8(sp)
    // sp
    ld sp,  1 * 8(sp)
    ld t0,  2 * 8(sp)
    ld t1,  3 * 8(sp)
    ld t2,  4 * 8(sp)
    ld a0,  5 * 8(sp)
    ld a1,  6 * 8(sp)
    ld a2,  7 * 8(sp)
    ld a3,  8 * 8(sp)
    ld a4,  9 * 8(sp)
    ld a5, 10 * 8(sp)
    ld a6, 11 * 8(sp)
    ld a7, 12 * 8(sp)
    ld t3, 13 * 8(sp)
    ld t4, 14 * 8(sp)
    ld t5, 15 * 8(sp)
    ld t6, 16 * 8(sp)
    ld s0, 17 * 8(sp)
    ld s1, 18 * 8(sp)
    ld s2, 19 * 8(sp)
    ld s3, 20 * 8(sp)
    ld s4, 21 * 8(sp)
    ld s5, 22 * 8(sp)
    ld s6, 23 * 8(sp)
    ld s7, 24 * 8(sp)
    ld s8, 25 * 8(sp)
    ld s9, 26 * 8(sp)
    ld s10,27 * 8(sp)
    ld s11,28 * 8(sp)
    ld gp, 29 * 8(sp)
    ld tp, 30 * 8(sp)
    
    addi sp, sp, 31 * 8
    
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
        sd t0, 31 *8(a0)

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

        ld t0, 31 *8(a1)
        csrw mstatus, t0
        
        ret