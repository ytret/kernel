#include <stdint.h>
#include <ytalloc/ytalloc.h>

#include "acpi/acpi.h"
#include "arch.h"
#include "arch_boot.h"
#include "arch_vmm.h"
#include "chardev.h"
#include "cmdline.h"
#include "config.h"
#include "conmgr.h"
#include "console.h"
#include "devmgr.h"
#include "fs/devfs.h"
#include "fs/ramfs.h"
#include "heap.h"
#include "inputmgr.h"
#include "kinttypes.h"
#include "libshim.h"
#include "log.h"
#include "panic.h"
#include "pmm.h"
#include "serial.h"
#include "smp.h"
#include "taskmgr.h"
#include "textdisp.h"
#include "tty.h"
#include "vfs/vnode.h"

#ifdef YTKERNEL_ENABLE_TESTS
#include "memfun.h"
#include "test/ktest.h"
#endif

static pmm_mmap_t g_mmap;
static pmm_region_t g_kernel_region;
static pmm_region_t g_cut_region;

static serial_ctx_t g_earlycon_serial;
static chardev_t g_earlycon_chardev;

static ramfs_ctx_t g_root_ramfs;

static void prv_main_add_kernel_region(pmm_mmap_t *mmap, paddr_t region_start,
                                       paddr_t region_end);
static void prv_main_mount_root_ramfs(void);
static void prv_main_mount_devfs(void);

void main(void) {
    libshim_init();
    arch_boot_init();

    g_earlycon_serial.port_base = SERIAL_COM1_BASE;
    g_earlycon_serial.baudrate_div = SERIAL_BAUDRATE_115200_DIV;
    if (serial_init(&g_earlycon_serial)) {
        serial_set_chardev(&g_earlycon_serial, &g_earlycon_chardev);
    }
    log_set_chardev(&g_earlycon_chardev);

    textdisp_t *const boot_disp = textdisp_get_boot_disp();
    arch_textdisp_early_init(boot_disp);

    LOG_INFO("Hello, World!");

    arch_early_init(&g_mmap);

    prv_main_add_kernel_region(&g_mmap, VMM_KERNEL_LMA,
                               arch_get_kernel_phys_end());
    pmm_init(&g_mmap);

    heap_init();
    arch_early_init_heap();

    textdisp_init(boot_disp);

    const char *const cmdline = arch_get_cmdline();
    cmdline_init(cmdline);

    console_t *const boot_con = console_get_boot_con();
    console_init(boot_con);
    console_attach(boot_con, boot_disp);

    tty_t *const boot_tty = tty_get_boot_tty();
    tty_init(boot_tty);
    tty_set_out(boot_tty, console_chardev(boot_con));

    inputmgr_init();
    inputmgr_set_tty(boot_tty);
    conmgr_init(9);
    LOG_INFO("console attached");

#ifdef YTKERNEL_ENABLE_TESTS
    serial_ctx_t *const ktest_serial = heap_alloc(sizeof(serial_ctx_t));
    chardev_t *const ktest_chardev = heap_alloc(sizeof(chardev_t));
    kmemset(ktest_chardev, 0, sizeof(*ktest_chardev));
    ktest_serial->port_base = SERIAL_COM2_BASE;
    ktest_serial->baudrate_div = SERIAL_BAUDRATE_115200_DIV;
    if (serial_init(ktest_serial)) {
        serial_set_chardev(ktest_serial, ktest_chardev);
    }
    ktest_set_chardev(ktest_chardev);

    ktest_announce();
    ktest_run_stage(KTEST_EARLY_BOOT);
#endif

    acpi_init();

    taskmgr_global_init();

    arch_late_init();

    vnode_root_init();
    prv_main_mount_root_ramfs();
    prv_main_mount_devfs();

#ifdef YTKERNEL_ENABLE_TESTS
    ktest_run_stage(KTEST_PRE_SMP);
#endif

    smp_init();
    // NOTE: main() is executed only by the bootstrap processor (BSP). Hence,
    // everything below is also executed only by the BSP.

    devmgr_init();

    taskmgr_local_init(arch_init_bsp_task);

    LOG_ERROR("end of main");
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

static void prv_main_mount_root_ramfs(void) {
    vnode_t *const root = vnode_root_node();
    if (!root) {
        PANIC("%s was called before root vnode is initialized", __func__);
    }
    if (root->fs_ctx) {
        LOG_ERROR("VFS root vnode is already mountpoint for some FS");
    }

    ramfs_init(&g_root_ramfs, 1024 * 1024);

    const vfs_err_t err = ramfs_get_desc()->f_mount(&g_root_ramfs, root);
    if (err != VFS_ERR_NONE) {
        LOG_ERROR("failed to mount root ramfs at /, error %d (%s)", err,
                  vfs_err_str(err));
        return;
    }
}

static void prv_main_mount_devfs(void) {
    vnode_t *const root = vnode_root_node();
    if (!root) {
        PANIC("%s was called before root vnode is initialized", __func__);
    }
    if (!root->ops) {
        LOG_ERROR("VFS root vnode has no ops");
        return;
    }
    if (!root->ops->f_mknode) {
        LOG_ERROR("VFS root vnode doesn't provide mknode op");
        return;
    }

    vnode_t *dev_dir;
    vfs_err_t err = root->ops->f_mknode(root, &dev_dir, "dev", VNODE_DIR);
    if (err != VFS_ERR_NONE) {
        LOG_ERROR("failed to create /dev directory, error %d (%s)", err,
                  vfs_err_str(err));
        return;
    }

    devfs_ctx_t *const devfs = devfs_global_ctx();
    devfs_init(devfs);

    err = devfs_get_desc()->f_mount(devfs, dev_dir);
    if (err != VFS_ERR_NONE) {
        LOG_ERROR("failed to mount devfs, error %d (%s)", err,
                  vfs_err_str(err));
    }
}
