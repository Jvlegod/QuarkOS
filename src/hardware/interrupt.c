#include "interrupt.h"

void interrupt_init() {
    plic_init();
}

void handler_external_interrupt() {
    handler_plic();
}