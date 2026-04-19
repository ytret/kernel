/**
 * @file kspinlock.h
 * A simple spinlock without task blocking or rescheduling.
 */

#pragma once

#include <stdatomic.h>

typedef volatile atomic_flag spinlock_t;

void spinlock_init(spinlock_t *spinlock);
void spinlock_acquire(spinlock_t *spinlock);
void spinlock_release(spinlock_t *spinlock);
