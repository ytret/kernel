#pragma once

typedef void *jmp_buf; // FIXME: made up

int setjmp(jmp_buf env);
[[noreturn]] void longjmp(jmp_buf env, int val);
