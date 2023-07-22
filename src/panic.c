#include <panic.h>
#include <printf.h>

#include <stdbool.h>

static volatile bool b_in_panic;

__attribute__ ((noreturn))
void
panic (char const * p_msg)
{
    __asm__ volatile ("cli");

    if (b_in_panic)
    {
	panic_silent();
    }

    b_in_panic = true;
    printf("Kernel panic: %s. Halting.\n", p_msg);

    for (;;)
    {}
}

__attribute__ ((noreturn))
void
panic_silent (void)
{
    __asm__ volatile ("cli");

    for (;;)
    {}
}
