/**
 * @file devdrv.c
 * Device enumeration and driver loading.
 */

#include "assert.h"
#include "devdrv.h"
#include "disk/ahci.h"
#include "kprintf.h"
#include "panic.h"
#include "pci.h"

/**
 * Maximum number of devices that are registerd in the kernel.
 * The number is chosen arbitrarily.
 */
#define DEVDRV_MAX_DEVS 32

static devdrv_dev_t g_devdrv_devs[DEVDRV_MAX_DEVS];
static size_t g_devdrv_num_devs;

static void prv_devdrv_init_dev(const pci_dev_t *pci_dev);
static void prv_devdrv_init_ahci(const pci_dev_t *pci_dev);

static devdrv_dev_t *prv_devdrv_next_dev(void);

void devdrv_init(void) {
    pci_init();

    const size_t num_devs = pci_num_devs();
    for (size_t idx_dev = 0; idx_dev < num_devs; idx_dev++) {
        if (g_devdrv_num_devs == DEVDRV_MAX_DEVS) {
            kprintf("devdrv_init: device limit (%u) has been reached\n",
                    DEVDRV_MAX_DEVS);
            break;
        }
        const pci_dev_t *pci_dev = pci_get_dev_const(idx_dev);
        prv_devdrv_init_dev(pci_dev);
    }
}

devdrv_dev_t *devdrv_find_by_class(devdrv_class_t dev_class) {
    for (size_t dev_idx = 0; dev_idx < g_devdrv_num_devs; dev_idx++) {
        devdrv_dev_t *const dev = &g_devdrv_devs[dev_idx];
        if (dev->dev_class == dev_class) { return dev; }
    }
    return NULL;
}

/**
 * Initializes a PCI device @a pci_dev if there is a driver for it.
 * @param pci_dev PCI device to load the driver(s) for.
 * @note
 * One PCI device may result in several kernel devices registered.
 */
static void prv_devdrv_init_dev(const pci_dev_t *pci_dev) {
    if (!pci_dev) { panic("prv_devdrv_init_dev: dev = NULL"); }

    const pci_header_common_t *const pci_header = &pci_dev->header.common;
    if (pci_header->base_class == PCI_BASE_CLASS_MASS_STORAGE) {
        if (pci_header->sub_class == PCI_MASS_STORAGE_SATA_DPA) {
            if (pci_header->interface == PCI_SATA_INTERFACE_AHCI) {
                kprintf(
                    "devdrv: pci %u-%u-%u: SATA DPA, AHBI HBA (major rev. 1)\n",
                    pci_dev->bus_num, pci_dev->dev_num, pci_dev->fun_num);
                prv_devdrv_init_ahci(pci_dev);
            } else {
                kprintf("devdrv: pci %u-%u-%u: unknown SATA DPA interface %u\n",
                        pci_dev->bus_num, pci_dev->dev_num, pci_dev->fun_num,
                        pci_header->interface);
            }

        } else {
            kprintf("devdrv: pci %u-%u-%u: unknown mass storage subclass %u\n",
                    pci_dev->bus_num, pci_dev->dev_num, pci_dev->fun_num,
                    pci_header->sub_class);
        }
    } else {
        kprintf("devdrv: pci %u-%u-%u: unknown base class %u\n",
                pci_dev->bus_num, pci_dev->dev_num, pci_dev->fun_num,
                pci_header->base_class);
    }
}

static void prv_devdrv_init_ahci(const pci_dev_t *pci_dev) {
    ASSERT(pci_dev->header.common.base_class == PCI_BASE_CLASS_MASS_STORAGE);
    ASSERT(pci_dev->header.common.sub_class == PCI_MASS_STORAGE_SATA_DPA);
    ASSERT(pci_dev->header.common.interface == PCI_SATA_INTERFACE_AHCI);

    ahci_ctrl_ctx_t *const ahci_ctrl = ahci_ctrl_new(pci_dev);
    if (!ahci_ctrl) {
        kprintf("devdrv: pci %u-%u-%u: failed to initialize ahci driver\n",
                pci_dev->bus_num, pci_dev->dev_num, pci_dev->fun_num);
        return;
    }

    for (size_t port_idx = 0; port_idx < AHCI_PORTS_PER_CTRL; port_idx++) {
        ahci_port_ctx_t *const ahci_port =
            ahci_ctrl_get_port(ahci_ctrl, port_idx);
        if (!ahci_port_is_online(ahci_port)) { continue; }

        devdrv_dev_t *const dev = prv_devdrv_next_dev();
        if (!dev) { return; }

        dev->dev_class = DEVDRV_CLASS_DISK;
        dev->driver_id = DEVDRV_DRIVER_AHCI_PORT;
        dev->driver_ctx = ahci_port;

        kprintf("devdrv: loaded driver for AHCI Port %s\n",
                ahci_port_name(ahci_port));
    }
}

static devdrv_dev_t *prv_devdrv_next_dev(void) {
    if (g_devdrv_num_devs < DEVDRV_MAX_DEVS) {
        devdrv_dev_t *const dev = &g_devdrv_devs[g_devdrv_num_devs];
        g_devdrv_num_devs++;
        return dev;
    } else {
        return NULL;
    }
}
