#ifndef __INTERRUPT_H__
#define __INTERRUPT_H__
#include "platform.h"
#include "plic.h"

#define TIMER_INTERVAL 10000000 // 1s

void interrupt_init();
void handler_external_interrupt();

#endif /* __INTERRUPT_H__ */