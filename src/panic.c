#include <panic.h>
#include <printf.h>

#include <stdbool.h>

static volatile bool b_in_panic;

void
panic (char const * p_msg)
{
    if (b_in_panic)
    {
	panic_silent();
    }

    b_in_panic = true;
    printf("Kernel panic: %s. Halting.\n", p_msg);

    for (;;)
    {}
}

void
panic_silent (void)
{
    for (;;)
    {}
}
