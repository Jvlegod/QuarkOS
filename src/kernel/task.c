#include "task.h"
#include "platform.h"
#include "kprintf.h"
#include "interrupt.h"
#include "hwtimer.h"

static struct task tasks[MAX_TASKS];
static int current_task = 0;
static int task_count = MAXNUM_CPU;

extern void trap_entry(void);
extern void start_switch_to(struct context *old);
extern void main();

static inline uint64_t* get_kernel_stack(int hartid) {
    extern uint8_t _kernel_stack_start[];
    uint8_t *base = _kernel_stack_start;

    /* calc：base + hartid * stack_size */
    return (uint64_t*)(base + 
        hartid * KERNEL_STACK_SIZE + 
        KERNEL_STACK_SIZE - 16);
}
static inline uint64_t* get_user_stack(int task_id) {
    extern uint8_t _user_stack_start[];
    uint8_t *base = _user_stack_start;

    /* calc：base + task_id * stack_size */
    return (uint64_t*)(base + 
        (task_id - 1) * USER_STACK_SIZE + 
        USER_STACK_SIZE - 16);
}

// idle task
static void idle_task() {
    task_create(main, 9);

    while(1) {
        __asm__ volatile ("wfi");
    }
}

static void init_idle_task(int hartid) {
    struct task *idle = &tasks[hartid];
    
    uint64_t *sp = get_kernel_stack(hartid);
    
    idle->ctx.ra = (uint64_t)idle_task;
    idle->ctx.sp = (uint64_t)sp;
    idle->ctx.mstatus = 0x1888;
    idle->ctx.mepc = (uint64_t)idle_task;
}

void task_init(int hartid) {

    for (int i = 0; i < MAX_TASKS; i ++) {
        tasks[i].status = TASK_UNCREATE;
    }

    init_idle_task(hartid);
    
    write_csr(mtvec, (uint64_t)trap_entry);
    start_switch_to(&tasks[hartid].ctx);
}

void task_create(void (*entry)(void), int task_id) {
    if(task_count >= MAX_TASKS) return;

    struct task *t = &tasks[task_count++];
    t->task_id = task_id;
    t->status = TASK_CREATE;

    uint64_t *sp = get_user_stack(task_id);

    t->ctx.ra = (uint64_t)entry;
    t->ctx.sp = (uint64_t)sp; // stack frame pointer
    t->ctx.mstatus = 0x0008; // MIE=0, MPP=00 (User mode)
}

void schedule() {
    task_yield();
}

void task_yield() {
    struct context *old = &tasks[current_task].ctx;
    struct context *new = NULL;
    
    for (int i = current_task; i < task_count; i ++) {
        current_task = (i + 1) % task_count;
        if (tasks[current_task].status == TASK_UNCREATE) {
            continue;
        }
        break;
    }

    new = &tasks[current_task].ctx;
    
    __asm__ volatile (
        "mv a0, %0\n"
        "mv a1, %1\n"
        "jal ctx_switch"
        :: "r"(old), "r"(new)
    );
}