#include "arch_boot.h"
#include "kstring.h"

#include "arch/x86/mbi.h"

#define ARCH_BOOT_MAX_MODS 32

static arch_boot_mod_t g_arch_boot_mods[ARCH_BOOT_MAX_MODS];
static size_t g_arch_boot_num_mods;

static arch_boot_fb_t g_arch_boot_fb;

void arch_boot_init(void) {
    const mbi_t *const p_mbi = mbi_ptr();

    if (p_mbi->flags & MBI_FLAG_FRAMEBUF) {
        g_arch_boot_fb.addr = p_mbi->framebuffer_addr;
        g_arch_boot_fb.height = p_mbi->framebuffer_height;
        g_arch_boot_fb.width = p_mbi->framebuffer_width;
        g_arch_boot_fb.pitch = p_mbi->framebuffer_pitch;
        g_arch_boot_fb.bpp = p_mbi->framebuffer_bpp;
    }

    for (size_t idx = 0; idx < mbi_num_mods(); idx++) {
        const mbi_mod_t *const mbi_mod = mbi_nth_mod(idx);

        arch_boot_mod_t *const arch_boot =
            &g_arch_boot_mods[g_arch_boot_num_mods];
        arch_boot->name = (const char *)mbi_mod->string;
        arch_boot->phys_start = mbi_mod->mod_start;
        arch_boot->phys_end = mbi_mod->mod_end;

        g_arch_boot_num_mods++;
    }
}

size_t arch_boot_num_mods(void) {
    return g_arch_boot_num_mods;
}

const arch_boot_fb_t *arch_boot_framebuf(void) {
    return &g_arch_boot_fb;
}

const arch_boot_mod_t *arch_boot_find_mod(const char *name) {
    for (size_t idx = 0; idx < g_arch_boot_num_mods; idx++) {
        const arch_boot_mod_t *const arch_boot = &g_arch_boot_mods[idx];
        if (string_equals(arch_boot->name, name)) { return arch_boot; }
    }
    return NULL;
}

const arch_boot_mod_t *arch_boot_nth_mod(size_t idx) {
    if (idx < g_arch_boot_num_mods) {
        return &g_arch_boot_mods[idx];
    } else {
        return NULL;
    }
}
