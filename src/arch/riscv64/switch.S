.section .text
.global ctx_switch

// void ctx_switch(struct context *old, struct context *new);
ctx_switch:
    sd ra, 0(a0)
    sd sp, 8(a0)
    sd s0, 16(a0)
    sd s1, 24(a0)
    sd s11, 104(a0)
    csrr t0, mstatus
    sd t0, 112(a0)

    ld ra, 0(a1)
    ld sp, 8(a1)
    ld s0, 16(a1)
    ld s1, 24(a1)
    ld s11, 104(a1)
    ld t0, 112(a1)
    csrw mstatus, t0

    ret