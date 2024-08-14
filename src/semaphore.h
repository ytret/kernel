#pragma once

#include "list.h"

typedef struct {
    int count;
    list_t waiting_tasks;
} semaphore_t;

void semaphore_init(semaphore_t *p_sem);
void semaphore_increase(semaphore_t *p_sem);
void semaphore_decrease(semaphore_t *p_sem);
