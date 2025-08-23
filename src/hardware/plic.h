#ifndef __PLIC_H__
#define __PLIC_H__
#include "platform.h"

#define KBD_IRQ 2
#define TABLET_IRQ 3
#define UART0_IRQ 10

#define MIE_MEIE (1 << 11) // external
#define MIE_MTIE (1 << 7)  // timer
#define MIE_MSIE (1 << 3)  // software

void plic_init();
void handler_plic();

#endif /* __PLIC_H__ */