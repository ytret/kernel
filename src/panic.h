#pragma once

void panic_enter(void);
[[gnu::noreturn]] void panic(char const *p_msg);
[[gnu::noreturn]] void panic_silent(void);
