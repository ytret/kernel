#pragma once

#include "kspinlock.h"
#include "list.h"

typedef struct {
    volatile int count;
    list_t waiting_tasks;
    spinlock_t list_lock;
} semaphore_t;

void semaphore_init(semaphore_t *p_sem);
void semaphore_increase(semaphore_t *p_sem);
void semaphore_decrease(semaphore_t *p_sem);
