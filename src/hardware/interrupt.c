#include "interrupt.h"
void timer_init() {
    write_csr(mie, read_csr(mie) | (1 << 7)); // MTIE
    write_csr(mstatus, read_csr(mstatus) | (1 << 3)); // MIE
    
    MTIMECMP = MTIME + 10000000;
}