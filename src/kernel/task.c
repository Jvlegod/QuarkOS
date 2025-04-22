#include "task.h"
#include "platform.h"
#include "uart.h"

static struct task tasks[MAX_TASKS];
static int current_task = 0;
static int task_count = 0;

// idle task
static void idle_task() {
    while(1) {
        __asm__ volatile ("wfi");
    }
}

void task_init() {
    write_csr(mtvec, (uint64_t)trap_entry);
    write_csr(mscratch, (uint64_t)&tasks[0]); // task0 as kernel task
    
    timer_init();

    task_create(idle_task, 0);
}

// task_id and entry always unique
void task_create(void (*entry)(void), int task_id) {
    if(task_count >= MAX_TASKS) return;

    struct task *t = &tasks[task_count++];
    t->task_id = task_id;

    uint64_t *sp = (uint64_t*)&t->stack[STACK_SIZE - 32];

    t->ctx.ra = (uint64_t)entry;
    t->ctx.sp = (uint64_t)sp; // stack frame pointer
    t->ctx.mstatus = 0x1800; // MPIE=1, MPP=00 (User mode)
    t->ctx.mepc = (uint64_t)entry; // expection ret addr
}

void timer_handler() {
    MTIMECMP = MTIME + 10000000; // 1s
    
    struct context *old = &tasks[current_task].ctx;
    current_task = (current_task + 1) % task_count;
    struct context *new = &tasks[current_task].ctx;
    
    write_csr(mscratch, (uint64_t)&tasks[current_task]);
    
    __asm__ volatile (
        "mv a0, %0\n"
        "mv a1, %1\n"
        "jal ctx_switch"
        :: "r"(old), "r"(new)
    );
}

void task_yield() {
    struct context *old = &tasks[current_task].ctx;
    
    current_task = (current_task + 1) % task_count;
    
    struct context *new = &tasks[current_task].ctx;
    __asm__ volatile (
        "mv a0, %0\n"
        "mv a1, %1\n"
        "jal ctx_switch"
        :: "r"(old), "r"(new)
    );
}