/**
 * @file devmgr.c
 * Device manager: enumeration and driver loading.
 */

#include "assert.h"
#include "blkdev/ahci.h"
#include "devmgr.h"
#include "kprintf.h"
#include "panic.h"
#include "pci.h"

/**
 * Maximum number of devices that are registerd in the kernel.
 * The number is chosen arbitrarily.
 */
#define DEVMGR_MAX_DEVS 32

/**
 * First ID to assign to the first identified device.
 * It cannot be 0 because ID 0 is indistinguishable from a free slot in
 * #g_devmgr_devs.
 */
#define DEVMGR_FIRST_ID 1
static_assert(DEVMGR_FIRST_ID > 0, "first ID cannot be NULL");

static devmgr_dev_t g_devmgr_devs[DEVMGR_MAX_DEVS];
static size_t g_devmgr_num_devs;
static uint32_t g_devmgr_next_id = DEVMGR_FIRST_ID;

static void prv_devmgr_init_dev(const pci_dev_t *pci_dev);
static void prv_devmgr_init_ahci(const pci_dev_t *pci_dev);

static devmgr_dev_t *prv_devmgr_init_next_dev(void);

void devmgr_init(void) {
    pci_init();

    const size_t num_devs = pci_num_devs();
    for (size_t idx_dev = 0; idx_dev < num_devs; idx_dev++) {
        if (g_devmgr_num_devs == DEVMGR_MAX_DEVS) {
            kprintf("devmgr_init: device limit (%u) has been reached\n",
                    DEVMGR_MAX_DEVS);
            break;
        }
        const pci_dev_t *pci_dev = pci_get_dev_const(idx_dev);
        prv_devmgr_init_dev(pci_dev);
    }
}

devmgr_dev_t *devmgr_get_by_id(uint32_t id) {
    for (size_t idx = 0; idx < g_devmgr_num_devs; idx++) {
        devmgr_dev_t *const dev = &g_devmgr_devs[idx];
        if (dev->id == id) { return dev; }
    }
    return NULL;
}

devmgr_dev_t *devmgr_find_by_class(devmgr_class_t dev_class) {
    for (size_t dev_idx = 0; dev_idx < g_devmgr_num_devs; dev_idx++) {
        devmgr_dev_t *const dev = &g_devmgr_devs[dev_idx];
        if (dev->dev_class == dev_class) { return dev; }
    }
    return NULL;
}

void devmgr_iter_init(devmgr_iter_t *iter, devmgr_class_t class_filter) {
    iter->class_filter = class_filter;
    iter->next_pos = 0;
}

devmgr_dev_t *devmgr_iter_next(devmgr_iter_t *iter) {
    devmgr_dev_t *dev = NULL;
    while (dev == NULL && iter->next_pos < g_devmgr_num_devs) {
        devmgr_dev_t *const it_dev = &g_devmgr_devs[iter->next_pos];
        if (iter->class_filter == DEVMGR_CLASS_NONE ||
            (iter->class_filter != DEVMGR_CLASS_NONE &&
             it_dev->dev_class == iter->class_filter)) {
            dev = it_dev;
        }
        iter->next_pos++;
    }
    return dev;
}

const char *devmgr_class_name(devmgr_class_t dev_class) {
    switch (dev_class) {
    case DEVMGR_CLASS_NONE:
        return "none";
    case DEVMGR_CLASS_DISK:
        return "disk";
    default:
        return "unknown";
    }
}

const char *devmgr_driver_name(devmgr_driver_t driver) {
    switch (driver) {
    case DEVMGR_DRVIER_NONE:
        return "none";
    case DEVMGR_DRIVER_AHCI_PORT:
        return "ahci port";
    default:
        return "unknown";
    }
}

/**
 * Initializes a PCI device @a pci_dev if there is a driver for it.
 * @param pci_dev PCI device to load the driver(s) for.
 * @note
 * One PCI device may result in several kernel devices registered.
 */
static void prv_devmgr_init_dev(const pci_dev_t *pci_dev) {
    if (!pci_dev) { panic("prv_devmgr_init_dev: dev = NULL"); }

    const pci_header_common_t *const pci_header = &pci_dev->header.common;
    if (pci_header->base_class == PCI_BASE_CLASS_MASS_STORAGE) {
        if (pci_header->sub_class == PCI_MASS_STORAGE_SATA_DPA) {
            if (pci_header->interface == PCI_SATA_INTERFACE_AHCI) {
                kprintf(
                    "devmgr: pci %u-%u-%u: SATA DPA, AHBI HBA (major rev. 1)\n",
                    pci_dev->bus_num, pci_dev->dev_num, pci_dev->fun_num);
                prv_devmgr_init_ahci(pci_dev);
            } else {
                kprintf("devmgr: pci %u-%u-%u: unknown SATA DPA interface %u\n",
                        pci_dev->bus_num, pci_dev->dev_num, pci_dev->fun_num,
                        pci_header->interface);
            }

        } else {
            kprintf("devmgr: pci %u-%u-%u: unknown mass storage subclass %u\n",
                    pci_dev->bus_num, pci_dev->dev_num, pci_dev->fun_num,
                    pci_header->sub_class);
        }
    } else {
        kprintf("devmgr: pci %u-%u-%u: unknown base class %u\n",
                pci_dev->bus_num, pci_dev->dev_num, pci_dev->fun_num,
                pci_header->base_class);
    }
}

static void prv_devmgr_init_ahci(const pci_dev_t *pci_dev) {
    ASSERT(pci_dev->header.common.base_class == PCI_BASE_CLASS_MASS_STORAGE);
    ASSERT(pci_dev->header.common.sub_class == PCI_MASS_STORAGE_SATA_DPA);
    ASSERT(pci_dev->header.common.interface == PCI_SATA_INTERFACE_AHCI);

    ahci_ctrl_ctx_t *const ahci_ctrl = ahci_ctrl_new(pci_dev);
    if (!ahci_ctrl) {
        kprintf("devmgr: pci %u-%u-%u: failed to initialize ahci driver\n",
                pci_dev->bus_num, pci_dev->dev_num, pci_dev->fun_num);
        return;
    }

    for (size_t port_idx = 0; port_idx < AHCI_PORTS_PER_CTRL; port_idx++) {
        ahci_port_ctx_t *const ahci_port =
            ahci_ctrl_get_port(ahci_ctrl, port_idx);
        if (!ahci_port_is_online(ahci_port)) { continue; }

        devmgr_dev_t *const dev = prv_devmgr_init_next_dev();
        if (!dev) { return; }

        dev->dev_class = DEVMGR_CLASS_DISK;
        dev->driver_id = DEVMGR_DRIVER_AHCI_PORT;
        dev->driver_ctx = ahci_port;

        ahci_port_set_int(ahci_port, AHCI_PORT_INT_ALL, true);

        kprintf("devmgr: loaded driver for AHCI Port %s\n",
                ahci_port_name(ahci_port));
    }

    ahci_ctrl_map_irq(ahci_ctrl, AHCI_VEC_GLOBAL);
    ahci_ctrl_set_int(ahci_ctrl, true);
}

/**
 * Acquires a slot with a unique ID in #g_devmgr_devs.
 */
static devmgr_dev_t *prv_devmgr_init_next_dev(void) {
    if (g_devmgr_num_devs < DEVMGR_MAX_DEVS) {
        devmgr_dev_t *const dev = &g_devmgr_devs[g_devmgr_num_devs];
        dev->id = g_devmgr_next_id;
        g_devmgr_num_devs++;
        g_devmgr_next_id++;
        return dev;
    } else {
        return NULL;
    }
}
