#include "interrupt.h"
#include "task.h"
#include "kprintf.h"

void timer_load(uint64_t intervel) {
    int hartid = read_tp();
    MTIMECMP(hartid) = MTIME + intervel;
}
void timer_init() {
    timer_load(TIMER_INTERVAL);
    write_csr(mie, read_csr(mie) | MIE_MTIE); // MTIE enable Timer int
}

void handle_timer_interrupt() {
    static int d = 0;
    kprintf("\n----- ticks %d -----\n", d);
    d ++;
    timer_load(TIMER_INTERVAL);
    schedule();
}