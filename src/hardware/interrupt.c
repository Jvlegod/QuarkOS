#include "interrupt.h"
void timer_init() {
    MTIMECMP = MTIME + TIMER_INTERVAL;

    write_csr(mie, read_csr(mie) | (1 << 7)); // MTIE enable Timer int
    write_csr(mstatus, read_csr(mstatus) | (1 << 3)); // MIE enable global int
}