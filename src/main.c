#include <stdint.h>
#include <ytalloc/ytalloc.h>

#include "acpi/acpi.h"
#include "arch.h"
#include "devmgr.h"
#include "heap.h"
#include "init.h"
#include "kinttypes.h"
#include "libshim.h"
#include "log.h"
#include "mbi.h"
#include "panic.h"
#include "pmm.h"
#include "serial.h"
#include "smp.h"
#include "taskmgr.h"
#include "term.h"
#include "vmm.h"

#define MULTIBOOT_MAGIC_NUM 0x2BADB002U

// See arch/x86/linker.ld.
extern uint32_t ld_vmm_kernel_start;
extern uint32_t ld_vmm_kernel_end;

static pmm_mmap_t g_mmap;
static pmm_region_t g_kernel_region;
static pmm_region_t g_cut_region;

static void check_bootloader(uint32_t magic_num, uint32_t mbi_addr);
static paddr_t prv_main_find_kernel_phys_end(void);
static void prv_main_add_kernel_region(pmm_mmap_t *mmap, paddr_t region_start,
                                       paddr_t region_end);

void main(uint32_t magic_num, uint32_t mbi_addr) {
    libshim_init();

    mbi_init((paddr_t)mbi_addr);

    serial_init();
    term_init();

    LOG_INFO("Hello, World!");
    check_bootloader(magic_num, mbi_addr);

    arch_init_1();

    if (!mbi_fill_mmap(mbi_ptr(), &g_mmap)) {
        PANIC("failed to fill the memory map");
    }

    const paddr_t kernel_start = VMM_KERNEL_LMA;
    const paddr_t kernel_end = prv_main_find_kernel_phys_end();
    LOG_DEBUG("kernel region physical 0x%016llx..0x%016llx", kernel_start,
              kernel_end);
    prv_main_add_kernel_region(&g_mmap, kernel_start, kernel_end);
    pmm_init(&g_mmap);

    heap_init();
    mbi_save_on_heap();

    term_init_history();
    term_clear();

    acpi_init();

    taskmgr_global_init();

    arch_init_2();

    smp_init();
    // NOTE: main() is executed only by the bootstrap processor (BSP). Hence,
    // everything below is also executed only by the BSP.

    devmgr_init();

    taskmgr_local_init(init_bsp_task);
    LOG_ERROR("end of main");
}

static void check_bootloader(uint32_t magic_num, uint32_t mbi_addr) {
    if (MULTIBOOT_MAGIC_NUM == magic_num) {
        LOG_INFO("booted by a multiboot-compliant bootloader");
        LOG_DEBUG("multiboot information structure is at 0x%08" PRIx32,
                  mbi_addr);
    } else {
        LOG_ERROR("main: magic number: 0x%" PRIx32 ", expected: 0x%x",
                  magic_num, MULTIBOOT_MAGIC_NUM);
        PANIC("booted by an unknown bootloader");
    }
}

static paddr_t prv_main_find_kernel_phys_end(void) {
    const mbi_mod_t *const last_mod = mbi_last_mod();
    const paddr_t kernel_end = VIRT_TO_PHYS(&ld_vmm_kernel_end);

    paddr_t last_used_addr;
    if (last_mod) { last_used_addr = last_mod->mod_end; }
    if (!last_mod || kernel_end > last_used_addr) {
        last_used_addr = kernel_end;
    }

    return (last_used_addr + 0x3FFFFFULL) & ~(0X3FFFFFULL);
}

static void prv_main_add_kernel_region(pmm_mmap_t *mmap, paddr_t region_start,
                                       paddr_t region_end_excl) {
    if (region_end_excl == 0) {
        PANIC("invalid argument 'region_end_excl' value 0");
    }

    const uintptr_t region_end_incl = region_end_excl - 1;

    // It is assumed that [region_start, region_end_excl) are residing in a
    // single memory map entry. Otherwise this function will fail.
    pmm_region_t *found_region;
    LIST_FIND(&mmap->entry_list, found_region, pmm_region_t, node,
              region->start <= region_start &&
                  region_end_incl <= region->end_incl,
              region);
    if (found_region) {
        LOG_FLOW("found covering region physical 0x%016llx..0x%016llx",
                 found_region->start, found_region->end_incl);
    } else {
        PANIC("TODO: could not find a single region that covers [0x%08llx; "
              "0x%08llx)",
              region_start, region_end_excl);
    }

    g_kernel_region.start = region_start;
    g_kernel_region.end_incl = region_end_incl;
    g_kernel_region.type = PMM_REGION_KERNEL_AND_MODS;

    LOG_DEBUG("insert kernel region physical 0x%016llx..0x%016llx",
              g_kernel_region.start, g_kernel_region.end_incl);

    if (found_region->start == region_start) {
        PANIC("TODO: case when the kernel region starts at the found region "
              "start");
    } else if (found_region->end_incl == region_end_incl) {
        PANIC("TODO: case when the kernel region ends at the found region end");
    } else {
        // Before: [              found_region             ]
        // After:  [found_region][kernel_region][cut_region]
        LOG_FLOW("split the found region into three parts");

        g_cut_region.start = g_kernel_region.end_incl + 1;
        g_cut_region.end_incl = found_region->end_incl;
        g_cut_region.type = PMM_REGION_AVAILABLE;

        found_region->end_incl = g_kernel_region.start - 1;

        list_insert(&mmap->entry_list, found_region->node.p_next,
                    &g_cut_region.node);
        list_insert(&mmap->entry_list, &g_cut_region.node,
                    &g_kernel_region.node);

        LOG_FLOW("first part  0x%016llx..0x%016llx", found_region->start,
                 found_region->end_incl);
        LOG_FLOW("kernel part 0x%016llx..0x%016llx", g_kernel_region.start,
                 g_kernel_region.end_incl);
        LOG_FLOW("third part  0x%016llx..0x%016llx", g_cut_region.start,
                 g_cut_region.end_incl);
    }
}
