#include "spinlock.h"

void spinlock_init(spinlock_t *spinlock) {
    atomic_flag_clear(spinlock);
}

void spinlock_acquire(spinlock_t *spinlock) {
    while (atomic_flag_test_and_set_explicit(spinlock, memory_order_acquire)) {
        __asm__ volatile("pause");
    }
}

void spinlock_release(spinlock_t *spinlock) {
    atomic_flag_clear_explicit(spinlock, memory_order_release);
}
