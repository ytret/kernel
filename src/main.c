#include <stdint.h>
#include <ytalloc/ytalloc.h>

#include "acpi/acpi.h"
#include "arch.h"
#include "cmdline.h"
#include "config.h"
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

#ifdef YTKERNEL_ENABLE_TESTS
#include "test/ktest.h"
#include "test/vmctl.h"
#endif

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

    prv_main_add_kernel_region(&g_mmap, VMM_KERNEL_LMA,
                               prv_main_find_kernel_phys_end());
    pmm_init(&g_mmap);

    heap_init();

    mbi_save_on_heap();
    if (mbi_ptr()->flags & MBI_FLAG_CMDLINE) {
        cmdline_init((const char *)mbi_ptr()->cmdline);
    }

    term_init_history();
    term_clear();

    ktest_run_stage(KTEST_EARLY_BOOT);

    acpi_init();

    taskmgr_global_init();

    arch_init_2();

    ktest_run_stage(KTEST_PRE_SMP);

    smp_init();
    // NOTE: main() is executed only by the bootstrap processor (BSP). Hence,
    // everything below is also executed only by the BSP.

    ktest_run_stage(KTEST_SMP);

    devmgr_init();

    int ktest_exitcode;
    if (ktest_should_exit_at_end(&ktest_exitcode)) {
        const ktest_globalctx_t *const ktest_ctx = ktest_get_globalctx();
        const int ratio_x1000 =
            ktest_ctx->tests_passed * 1000 / ktest_ctx->tests_run;

        LOG_INFO("ktest results:");
        LOG_INFO("%zu.%zu%% tests passed, %zu failed out of %zu",
                 ratio_x1000 / (size_t)10, ratio_x1000 % (size_t)10,
                 ktest_ctx->tests_failed, ktest_ctx->tests_run);
        vmctl_exit(ktest_exitcode);
    } else {
        taskmgr_local_init(init_bsp_task);
    }

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

    g_kernel_region.start = region_start;
    g_kernel_region.end_incl = region_end_incl;

    LOG_DEBUG("insert kernel region physical 0x%016llx..0x%016llx",
              g_kernel_region.start, g_kernel_region.end_incl);
    pmm_mmap_insert(mmap, &g_kernel_region, &g_cut_region);

    g_kernel_region.type = PMM_REGION_KERNEL_AND_MODS;
    g_cut_region.type = PMM_REGION_AVAILABLE;
    // NOTE: it is assumed that the original region covering the kernel is
    // marked as available RAM, therefore #g_cut_region is marked as such too.
}
