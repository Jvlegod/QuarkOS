#include "syscall.h"

enum {
    SYS_GETUID = 0,
    SYS_SETUID = 1,
};

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