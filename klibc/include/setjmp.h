#pragma once

typedef void *jmp_buf[5];

int setjmp(jmp_buf env);
[[noreturn]] void longjmp(jmp_buf env, int val);
