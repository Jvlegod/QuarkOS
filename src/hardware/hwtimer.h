#ifndef __TIMER_H__
#define __TIMER_H__
#include "ktypes.h"

void timer_load(uint64_t intervel);
void timer_init();
void handle_timer_interrupt();

#endif /* __TIMER_H__ */