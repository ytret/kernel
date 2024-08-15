#pragma once

void panic_enter(void);
void panic(char const *p_msg) __attribute__((noreturn));
void panic_silent(void) __attribute__((noreturn));
