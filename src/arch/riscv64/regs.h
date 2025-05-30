#ifndef __REGS_H__
#define __REGS_H__

static inline void intr_on() {
    asm volatile("csrsi mstatus, 0x8");
}

static inline void intr_off() {
    asm volatile("csrci mstatus, 0x8"); 
}

#define write_csr(csr, val) ({ \
    __asm__ volatile ("csrw " #csr ", %0" :: "r"(val)); \
})

#define read_csr(csr) ({ \
    unsigned long __v; \
    __asm__ volatile ("csrr %0, " #csr : "=r"(__v)); \
    __v; \
})

#define read_tp() ({ \
    unsigned long __value; \
    __asm__ volatile ("mv %0, tp" : "=r"(__value)); \
    __value; \
})

static inline uint64_t atomic_load_u64(const volatile uint64_t *ptr) {
    uint64_t val;
    __asm__ volatile (
        "ld %0, %1\n"
        "fence ir, ir"
        : "=r" (val)
        : "A" (*ptr)
        : "memory"
    );
    return val;
}

#endif /* __REGS_H__ */