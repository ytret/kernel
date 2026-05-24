/**
 * @file init.h
 * SMP-aware initial tasks.
 */

#pragma once

[[gnu::noreturn]] void init_bsp_task_common(void);
[[gnu::noreturn]] void init_ap_task_common(void);
