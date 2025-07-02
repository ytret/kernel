#include <stddef.h>
#include <stdint.h>

#include "framebuf.h"
#include "heap.h"
#include "kprintf.h"
#include "mbi.h"
#include "memfun.h"
#include "panic.h"
#include "smp.h"
#include "vmm.h"

#define VMM_ADDR_DIR_IDX(addr) (((addr) >> 22) & 0x3FF)
#define VMM_ADDR_TBL_IDX(addr) (((addr) >> 12) & 0x3FF)

#define VMM_TABLE_USER    (1 << 2)
#define VMM_TABLE_RW      (1 << 1)
#define VMM_TABLE_PRESENT (1 << 0)

#define VMM_PAGE_USER    (1 << 2)
#define VMM_PAGE_RW      (1 << 1)
#define VMM_PAGE_PRESENT (1 << 0)

/**
 * Table entry flags that are checked when comparing two page tables.
 * If the two page table entries have different values when this mask is
 * applied, they are considered different.
 */
#define VMM_TBL_EQ_FLAGS (VMM_TABLE_USER | VMM_TABLE_RW | VMM_TABLE_PRESENT)

static uint32_t *gp_kvas_dir;

static void map_page(uint32_t *p_dir, uint32_t virt, uint32_t phys,
                     uint32_t flags);
static void unmap_page(uint32_t *p_dir, uint32_t virt);

void vmm_init(void) {
    mbi_t const *p_mbi = mbi_ptr();

    gp_kvas_dir = heap_alloc_aligned(4096, 4096);
    __builtin_memset(gp_kvas_dir, 0, 4096);
    kprintf("vmm: kernel page dir is at %P\n", gp_kvas_dir);

    // Identity map everything from the first page to heap end.
    uint32_t map_start = 0x1000;
    uint32_t map_end = (heap_end() + 0xFFF) & ~0xFFF;
    for (uint32_t page = map_start; page < map_end; page += 4096) {
        vmm_map_kernel_page(page, page);
    }

    // If the framebuffer is still not mapped, map it.
    if ((p_mbi->flags & MBI_FLAG_FRAMEBUF) &&
        (p_mbi->framebuffer_type != MBI_FRAMEBUF_EGA)) {
        size_t size = (p_mbi->framebuffer_height * p_mbi->framebuffer_pitch);
        uint64_t framebuf_end = (p_mbi->framebuffer_addr + size);
        if (framebuf_end > 0x100000000) {
            // We allow framebuf_end to be at 0x1_0000_000, but not past that
            // point.
            panic_enter();
            kprintf("vmm: vmm_init: framebuffer end is beyond 4 GiBs at"
                    " 0x%08X_%08X\n",
                    ((uint32_t)(framebuf_end >> 32)), ((uint32_t)framebuf_end));
            panic("framebuffer is too large");
        }

        for (uint32_t virt = p_mbi->framebuffer_addr; virt < framebuf_end;
             virt += 4096) {
            uint32_t phys = virt;
            vmm_map_kernel_page(virt, phys);
        }
    }

    // Enable paging.
    vmm_load_dir(gp_kvas_dir);

    kprintf("vmm: memory range %P..%P is identity mapped\n", map_start,
            map_end);
}

uint32_t const *vmm_kvas_dir(void) {
    return gp_kvas_dir;
}

uint32_t *vmm_clone_kvas_dir(void) {
    uint32_t *const p_dir = heap_alloc_aligned(4096, 4096);
    kmemset(p_dir, 0, 4096);

    for (uint32_t dir_idx = 0; dir_idx < VMM_ADDR_DIR_IDX(VMM_USER_START);
         dir_idx++) {
        if ((gp_kvas_dir[dir_idx] & VMM_TABLE_PRESENT) == 0) { continue; }

        p_dir[dir_idx] = gp_kvas_dir[dir_idx];
    }

    // Clone the video framebuffer.  If the framebuffer is text-only, both of
    // these variables are zero, and no mapping is performed.
    uint32_t fb_start_page = framebuf_start() & ~0xFFF;
    uint32_t fb_end_page = (framebuf_end() + 0xFFF) & ~0xFFF;
    for (uint32_t fb_page = fb_start_page; fb_page < fb_end_page;
         fb_page += 4096) {
        map_page(p_dir, fb_page, fb_page, (VMM_PAGE_RW | VMM_PAGE_PRESENT));
    }

    return p_dir;
}

void vmm_free_vas(uint32_t *p_dir) {
    (void)p_dir;
}

void vmm_map_user_page(uint32_t *p_dir, uint32_t virt, uint32_t phys) {
    map_page(p_dir, virt, phys,
             (VMM_PAGE_USER | VMM_PAGE_RW | VMM_PAGE_PRESENT));
}

void vmm_map_kernel_page(uint32_t virt, uint32_t phys) {
    map_page(gp_kvas_dir, virt, phys, (VMM_PAGE_RW | VMM_PAGE_PRESENT));
    vmm_invlpg(virt);
    if (smp_is_active()) { smp_send_tlb_shootdown(virt); }
}

void vmm_unmap_kernel_page(uint32_t virt) {
    unmap_page(gp_kvas_dir, virt);
    vmm_invlpg(virt);
    if (smp_is_active()) { smp_send_tlb_shootdown(virt); }
}

void vmm_invlpg(uint32_t virt) {
    __asm__ volatile("invlpg (%0)"
                     : /* no outputs */
                     : "r"(virt)
                     : "memory");
}

static void map_page(uint32_t *p_dir, uint32_t virt, uint32_t phys,
                     uint32_t flags) {
    if (!p_dir) {
        panic_enter();
        kprintf("vmm: map_page: p_dir = NULL\n");
        panic("invalid argument");
    }
    if (virt & 0xFFF) {
        panic_enter();
        kprintf("vmm: map_page: virt is not page-aligned\n");
        panic("invalid argument");
    }
    if (phys & 0xFFF) {
        panic_enter();
        kprintf("vmm: map_page: phys is not page-aligned\n");
        panic("invalid argument");
    }

    uint32_t dir_idx = VMM_ADDR_DIR_IDX(virt);
    uint32_t tbl_idx = VMM_ADDR_TBL_IDX(virt);

    uint32_t *p_tbl;
    if (p_dir[dir_idx] & VMM_TABLE_PRESENT) {
        if ((p_dir[dir_idx] & VMM_TBL_EQ_FLAGS) != flags) {
            panic_enter();
            kprintf("vmm: map_page: page table for %P is present, but its"
                    " checked flags 0x%03x are different from 0x%03x\n",
                    virt, (p_dir[dir_idx] & VMM_TBL_EQ_FLAGS), flags);
            panic("unexpected behavior");
        }

        p_tbl = ((uint32_t *)(p_dir[dir_idx] & ~0xFFF));
    } else {
        // Allocate a new page table.
        p_tbl = heap_alloc_aligned(4096, 4096);
        __builtin_memset(p_tbl, 0, 4096);

        // Fill the dir entry.
        p_dir[dir_idx] = (((uint32_t)p_tbl) | flags);
    }

    if (p_tbl[tbl_idx] != 0) {
        // The table entry is not empty.
        panic_enter();
        kprintf("vmm: map_page: table entry %u for %P is not empty\n", tbl_idx,
                ((void *)virt));
        panic("unexpected behavior");
    }

    p_tbl[tbl_idx] = (phys | flags);
}

static void unmap_page(uint32_t *p_dir, uint32_t virt) {
    if (virt & 0xFFF) {
        panic_enter();
        kprintf("vmm: unmap_page: virt is not page-aligned\n");
        panic("invalid argument");
    }

    uint32_t dir_idx = VMM_ADDR_DIR_IDX(virt);
    uint32_t tbl_idx = VMM_ADDR_TBL_IDX(virt);

    if (!(p_dir[dir_idx] & VMM_TABLE_PRESENT)) {
        panic_enter();
        kprintf("vmm: unmap_page: table %u for %P is not present\n", dir_idx,
                ((void *)virt));
        panic("unexpected behavior");
    }

    uint32_t *p_tbl = ((uint32_t *)(p_dir[dir_idx] & ~0xFFF));

    if (!(p_tbl[tbl_idx] & VMM_PAGE_PRESENT)) {
        panic_enter();
        kprintf("vmm: unmap_page: page %u for %P is not present\n", tbl_idx,
                ((void *)virt));
        panic("unexpected behavior");
    }

    p_tbl[tbl_idx] = 0;
}
