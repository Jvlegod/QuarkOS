#include "plic.h"
#include "kprintf.h"
static inline int plic_claim() {
    int irq = *(uint32_t*)PLIC_MCLAIM(read_tp());
    return irq;
}

static inline void plic_complete(int irq) {
    *(uint32_t*)PLIC_MCOMPLETE(read_tp()) = irq;
}

void plic_init() {
    int hart = read_tp();
    *(uint32_t*)PLIC_PRIORITY(UART0_IRQ) = 1;
    *(uint32_t*)PLIC_MENABLE(hart, UART0_IRQ)= (1 << (UART0_IRQ % 32));
    *(uint32_t*)PLIC_MTHRESHOLD(hart) = 0;
    write_csr(mie, read_csr(mie) | MIE_MEIE);
    intr_on();
}

void handler_plic() {
    int irq = plic_claim();
    if (irq == UART0_IRQ) {

    }

    if (irq) {
        plic_complete(irq);
    }
}