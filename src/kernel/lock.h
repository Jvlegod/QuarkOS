#ifndef __LOCK_H__
#define __LOCK_H__
#include "ktypes.h"
#include "regs.h"

struct semaphore {
    uint8_t value __attribute__((aligned(8)));
};

struct spinlock {
    struct semaphore sem;
};

void init_spinlock(struct spinlock *lock, uint8_t num);
void spin_lock(struct spinlock *lock);
void spin_unlock(struct spinlock *lock);

#endif // __LOCK_H__