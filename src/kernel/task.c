#include "task.h"
#include "platform.h"
#include "kprintf.h"
#include "interrupt.h"
#include "hwtimer.h"

static struct task tasks[MAX_TASKS];
static int current_task = 0;
static int task_count = MAXNUM_CPU;

extern void trap_entry(void);
extern void ctx_int_switch(struct context *new);
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
    task_create(main, (void *)NULL, 0);

    while(1) {
        __asm__ volatile ("wfi");
    }
}
static void init_idle_task(int hartid) {
    struct task *idle = &tasks[hartid];
    
    uint64_t *sp = get_kernel_stack(hartid);
    
    // riscv64
    idle->ctx.ra = (uint64_t)idle_task;
    idle->ctx.sp = (uint64_t)sp;
    // MIE = 0
    // MPIE = 1
    // MPP = 3 (Machine mode)
    idle->ctx.mstatus = 0x1880;
    idle->ctx.mepc = (uint64_t)idle_task;
    idle->uid = 0;
}

void task_init(int hartid) {

    for (int i = 0; i < MAX_TASKS; i ++) {
        tasks[i].status = TASK_UNCREATE;
        tasks[i].uid = 0;
    }

    init_idle_task(hartid);
    
    write_csr(mtvec, (uint64_t)trap_entry);
    ctx_int_switch(&tasks[hartid].ctx);
}

void task_create(void (*entry)(void*), void *arg, int uid) {
    if(task_count >= MAX_TASKS) return;

    struct task *t = &tasks[task_count++];
    t->task_id = task_count;
    t->uid = uid;
    t->status = TASK_CREATE;

    uint64_t *sp = get_user_stack(task_count);

    // riscv64
    t->ctx.a0 = (uint64_t)arg;
    t->ctx.ra = (uint64_t)entry;
    t->ctx.sp = (uint64_t)sp; // stack frame pointer
    t->ctx.mstatus = 0x0080; // MPP=00 (User mode)
    t->ctx.mepc = (uint64_t)entry;
    task_count++;
}

void task_exit() {
    struct task *t = &tasks[current_task];
    t->status = TASK_UNCREATE;
}

void schedule() {
    // task_yield(); // here we can change proactive
    task_int_yield();
}

void task_int_yield() {
    struct context *new = NULL;
    
    for (int i = current_task; i < task_count; i ++) {
        current_task = (i + 1) % task_count;
        if (tasks[current_task].status == TASK_UNCREATE) {
            continue;
        }
        break;
    }

    new = &tasks[current_task].ctx;
    
    // riscv64
    __asm__ volatile (
        "mv a0, %0\n"
        "jal ctx_int_switch"
        :: "r"(new)
    );   
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
    
    // riscv64
    __asm__ volatile (
        "mv a0, %0\n"
        "mv a1, %1\n"
        "jal ctx_switch"
        :: "r"(old), "r"(new)
    );
}

const struct task* task_get_tasks(void) {
    return tasks;
}

int task_get_count(void) {
    return task_count;
}

int task_get_current_uid(void) {
    return tasks[current_task].uid;
}

void task_set_current_uid(int uid) {
    tasks[current_task].uid = uid;
}