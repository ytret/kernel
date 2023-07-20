#include <kbd.h>
#include <kshell.h>
#include <printf.h>

static void kbd_callback(uint8_t key, bool b_released);

void
kshell_init (void)
{
    kbd_set_callback(kbd_callback);

    for (;;)
    {}
}

static void
kbd_callback (uint8_t key, bool b_released)
{
    printf("%u %u\n", key, b_released);
}
