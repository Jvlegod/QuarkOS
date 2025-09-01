#include "syscall.h"

static long sys_getuid(void) {
    return task_get_current_uid();
}

static long sys_setuid(long uid) {
    task_set_current_uid((int)uid);
    return 0;
}


/*
 * a7: syscall number
 * a0~a6: parameters
 */
void handle_syscall(struct context *ctx) {
    switch (ctx->a7) {
    case SYS_GETUID:
        ctx->a0 = sys_getuid();
        break;
    case SYS_SETUID:
        ctx->a0 = sys_setuid(ctx->a0);
        break;
    default:
        break;
    }
}

long syscall(long n, long a0, long a1, long a2,
                     long a3, long a4, long a5)
{
    register long x0 asm("a0") = a0;
    register long x1 asm("a1") = a1;
    register long x2 asm("a2") = a2;
    register long x3 asm("a3") = a3;
    register long x4 asm("a4") = a4;
    register long x5 asm("a5") = a5;
    register long x7 asm("a7") = n;

    asm volatile ("ecall"
                  : "+r"(x0)
                  : "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5), "r"(x7)
                  : "memory");

    return x0;
}