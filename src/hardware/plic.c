#include "plic.h"
#include "kprintf.h"
#include "shell.h"
#include "virtio_keyboard.h"
#include "virtio_tablet.h"
#include "desktop.h"

extern virtio_kbd_t    g_kbd;
extern virtio_tablet_t g_tab;

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
    *(uint32_t*)PLIC_PRIORITY(KBD_IRQ)   = 1;
    *(uint32_t*)PLIC_PRIORITY(TABLET_IRQ)= 1;

    *(uint32_t*)PLIC_MENABLE(hart, UART0_IRQ)= (1 << (UART0_IRQ % 32));
    *(uint32_t*)PLIC_MENABLE(hart, KBD_IRQ/32)   |= (1u << (KBD_IRQ   % 32));
    *(uint32_t*)PLIC_MENABLE(hart, TABLET_IRQ/32)|= (1u << (TABLET_IRQ% 32));

    *(uint32_t*)PLIC_MTHRESHOLD(hart) = 0;
    write_csr(mie, read_csr(mie) | MIE_MEIE);
    intr_on();
}

void handler_plic() {
    int irq = plic_claim();
    if (irq == UART0_IRQ) {
        uart_isr();
    } else if (irq == KBD_IRQ) {
        virtio_kbd_irq(&g_kbd);
        desktop_poll_keyboard(&g_kbd);
    } else if (irq == TABLET_IRQ) {
        virtio_tablet_irq(&g_tab);
        desktop_poll_cursor(&g_tab);
    } else if (irq) {
        kprintf("unexpected interrupt irq = %d\r\n", irq);
    }

    if (irq) {
        plic_complete(irq);
    }
}