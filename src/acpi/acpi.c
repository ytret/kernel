#include <stddef.h>
#include <stdint.h>

#include "acpi/acpi.h"
#include "kprintf.h"
#include "memfun.h"

#define acpiBIOS_SEARCH_START 0xE0000
#define acpiBIOS_SEARCH_END   (0xFFFFF - 8)

void acpi_init(void) {
    bool found = false;
    const uint8_t *it_u8 = (uint8_t *)acpiBIOS_SEARCH_START;
    const uint8_t *const ptr_end = (uint8_t *)acpiBIOS_SEARCH_END;
    while (it_u8 < ptr_end) {
        const char *const it_str = (const char *)it_u8;
        const char signature[8] = "RSD PTR ";
        if (!kmemcmp(it_str, signature, sizeof(signature))) {
            kprintf("Found RSD PTR at %p\n", it_str);
            found = true;
            break;
        }
        it_u8++;
    }
    if (!found) {
        kprintf("acpi: could not find RSD PTR in 0x%08x...0x%08x\n",
                acpiBIOS_SEARCH_START, acpiBIOS_SEARCH_END);
    }
}
