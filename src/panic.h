#pragma once

void panic(char const *p_msg) __attribute__((noreturn));
void panic_silent(void) __attribute__((noreturn));
