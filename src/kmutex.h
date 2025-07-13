#pragma once

#include "kspinlock.h"
#include "taskmgr.h"

typedef struct {
    /// The task that has acquired the mutex.
    task_t *locking_task;

    /// List of tasks waiting to acquire the mutex.
    list_t waiting_tasks;

    /// Lock for synchronizing SMP-access to the waiting tasks list.
    spinlock_t list_lock;
} task_mutex_t;

void mutex_init(task_mutex_t *mutex);
void mutex_acquire(task_mutex_t *mutex);
void mutex_release(task_mutex_t *mutex);
bool mutex_caller_owns(task_mutex_t *mutex);
