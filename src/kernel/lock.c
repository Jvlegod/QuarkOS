#include "lock.h"

void init_spinlock(struct spinlock *lock, uint8_t num) {
    lock->sem.value = num;
}

void spin_lock(struct spinlock *lock) {
    for (;;) {
        if (atomic_load_u64((uint64_t*)&lock->sem.value) > 0) {
            __asm__ volatile (
                "amoadd.w zero, %1, %0\n"
                : "+A" (lock->sem.value)
                : "r" (-1)
                : "memory"
            );
            break;
        }
    }
}

void spin_unlock(struct spinlock *lock) {
    __asm__ volatile (
        "amoadd.w zero, %1, %0"
        : "+A" (lock->sem.value)
        : "r" (1)
        : "memory"
    );
}