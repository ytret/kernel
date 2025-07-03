/**
 * @file init.h
 * SMP-aware initial tasks.
 */

#pragma once

[[gnu::noreturn]] void init_bsp_task(void);
[[gnu::noreturn]] void init_ap_task(void);
