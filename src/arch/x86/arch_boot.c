#include "arch_boot.h"
#include "dynarr.h"
#include "kstring.h"

#include "arch/x86/mbi.h"

static dynarr_t g_arch_boot_mods;
static size_t g_arch_boot_num_mods;

void arch_boot_init(void) {
    dynarr_init(&g_arch_boot_mods, sizeof(arch_boot_mod_t),
                _Alignof(arch_boot_mod_t), 0);

    for (size_t idx = 0; idx < mbi_num_mods(); idx++) {
        const mbi_mod_t *const mbi_mod = mbi_nth_mod(idx);

        arch_boot_mod_t arch_boot = {0};
        arch_boot.name = (const char *)mbi_mod->string;
        arch_boot.phys_start = mbi_mod->mod_start;
        arch_boot.phys_end = mbi_mod->mod_end;

        dynarr_push(&g_arch_boot_mods, &arch_boot, NULL);
    }
}

size_t arch_boot_num_mods(void) {
    return g_arch_boot_num_mods;
}

const arch_boot_mod_t *arch_boot_find_mod(const char *name) {
    for (size_t idx = 0; idx < g_arch_boot_mods.num_items; idx++) {
        const arch_boot_mod_t *const arch_boot =
            dynarr_ptr_at(&g_arch_boot_mods, idx);
        if (string_equals(arch_boot->name, name)) { return arch_boot; }
    }
    return NULL;
}

const arch_boot_mod_t *arch_boot_nth_mod(size_t idx) {
    const arch_boot_mod_t *const arch_boot =
        dynarr_ptr_at(&g_arch_boot_mods, idx);
    return arch_boot;
}
