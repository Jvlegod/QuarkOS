#include "task.h"
#include "platform.h"
#include "uart.h"
#include "interrupt.h"

static struct task tasks[MAX_TASKS];
static int current_task = 0;
static int task_count = MAXNUM_CPU;

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
        task_id * USER_STACK_SIZE + 
        USER_STACK_SIZE - 16);
}

// idle task
static void idle_task() {
    while(1) {
        __asm__ volatile ("wfi");
    }
}

static void init_idle_task(int hartid) {
    struct task *idle = &tasks[hartid];
    
    uint64_t *sp = get_kernel_stack(hartid);
    
    idle->ctx.ra = (uint64_t)idle_task;
    idle->ctx.sp = (uint64_t)sp;
    idle->ctx.mstatus = 0x1800; // MPIE=1, MPP=Machine mode
    idle->ctx.mepc = (uint64_t)idle_task;
}

void task_init(int hartid) {

    for (int i = 0; i < MAX_TASKS; i ++) {
        tasks[i].status = TASK_UNCREATE;
    }
    
    // write_csr(mtvec, (uint64_t)trap_entry);
    // write_csr(mscratch, (uint64_t)&tasks[hartid].ctx); // task0 as kernel task
    // timer_init();

    init_idle_task(hartid);
}

void task_create(void (*entry)(void), int task_id) {
    if(task_count >= MAX_TASKS) return;

    struct task *t = &tasks[task_count++];
    t->task_id = task_id;
    t->status = TASK_CREATE;

    uint64_t *sp = get_user_stack(task_id);

    t->ctx.ra = (uint64_t)entry;
    t->ctx.sp = (uint64_t)sp; // stack frame pointer
    t->ctx.mstatus = 0x1800; // MPIE=1, MPP=00 (User mode)
    t->ctx.mepc = (uint64_t)entry; // expection ret addr
}

void timer_handler(struct context *ctx) {
    MTIMECMP = MTIME + TIMER_INTERVAL; // 1s
    
    // uart_printf("\n----- Context Dump -----\n");
    // uart_printf("ra  (x1 ): 0x%x\n", ctx->ra);
    // uart_printf("sp  (x2 ): 0x%x\n", ctx->sp);
    // uart_printf("t0  (x5 ): 0x%x\n", ctx->t0);
    // uart_printf("t1  (x6 ): 0x%x\n", ctx->t1);
    // uart_printf("t2  (x7 ): 0x%x\n", ctx->t2);
    // uart_printf("a0  (x10): 0x%x\n", ctx->a0);
    // uart_printf("a1  (x11): 0x%x\n", ctx->a1);
    // uart_printf("a2  (x12): 0x%x\n", ctx->a2);
    // uart_printf("a3  (x13): 0x%x\n", ctx->a3);
    // uart_printf("a4  (x14): 0x%x\n", ctx->a4);
    // uart_printf("a5  (x15): 0x%x\n", ctx->a5);
    // uart_printf("a6  (x16): 0x%x\n", ctx->a6);
    // uart_printf("a7  (x17): 0x%x\n", ctx->a7);
    // uart_printf("t3  (x28): 0x%x\n", ctx->t3);
    // uart_printf("t4  (x29): 0x%x\n", ctx->t4);
    // uart_printf("t5  (x30): 0x%x\n", ctx->t5);
    // uart_printf("t6  (x31): 0x%x\n", ctx->t6);
    // uart_printf("s0  (x8 ): 0x%x\n", ctx->s0);
    // uart_printf("s1  (x9 ): 0x%x\n", ctx->s1);
    // uart_printf("s2  (x18): 0x%x\n", ctx->s2);
    // uart_printf("s3  (x19): 0x%x\n", ctx->s3);
    // uart_printf("s4  (x20): 0x%x\n", ctx->s4);
    // uart_printf("s5  (x21): 0x%x\n", ctx->s5);
    // uart_printf("s6  (x22): 0x%x\n", ctx->s6);
    // uart_printf("s7  (x23): 0x%x\n", ctx->s7);
    // uart_printf("s8  (x24): 0x%x\n", ctx->s8);
    // uart_printf("s9  (x25): 0x%x\n", ctx->s9);
    // uart_printf("s10 (x26): 0x%x\n", ctx->s10);
    // uart_printf("s11 (x27): 0x%x\n", ctx->s11);
    // uart_printf("gp  (x3 ): 0x%x\n", ctx->gp);
    // uart_printf("tp  (x4 ): 0x%x\n", ctx->tp);
    // uart_printf("mstatus: 0x%x\n", ctx->mstatus);
    // uart_printf("mepc   : 0x%x\n", ctx->mepc);
    // uart_printf("mcause : 0x%x\n", ctx->mcause);
    // uart_printf("mtval  : 0x%x\n", ctx->mtval);
    // uart_printf("-----------------------\n");
}

void task_yield() {
    struct context *old = &tasks[current_task].ctx;
    struct context *new = NULL;
    
    for (int i = current_task; i < task_count; i ++) {
        current_task = (current_task + 1) % task_count;
        if (tasks[current_task].status == TASK_UNCREATE) {
            continue;
        }
    }

    new = &tasks[current_task].ctx;
    
    __asm__ volatile (
        "mv a0, %0\n"
        "mv a1, %1\n"
        "jal ctx_switch"
        :: "r"(old), "r"(new)
    );
}