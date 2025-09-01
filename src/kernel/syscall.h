#ifndef __SYSCALL_H__
#define __SYSCALL_H__
#include "task.h"

enum {
    SYS_GETUID = 0,
    SYS_SETUID = 1,
};

void handle_syscall(struct context *ctx);
long syscall(long n, long a0, long a1, long a2,
                            long a3, long a4, long a5);

#endif /* __SYSCALL_H__ */