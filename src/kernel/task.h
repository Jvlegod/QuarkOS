#ifndef __TASK_H__
#define __TASK_H__
#include "list.h"

#define MAX_TASKS 4
#define STACK_SIZE 4096

// task context
struct context {
    uint64_t ra;
    uint64_t sp;
    /* temp ignore gp, tp, t0-t2 */
    uint64_t s[12]; // s0-s11
    /* temp ignore t3-t6 */
    uint64_t mstatus;
};

struct task {
    uint8_t stack[STACK_SIZE]; // task stack
    struct context ctx;        // task context
    int task_id;
};

void task_init();
void task_create(void (*entry)(void), int task_id);
void task_yield();

#endif /* __TASK_H__ */